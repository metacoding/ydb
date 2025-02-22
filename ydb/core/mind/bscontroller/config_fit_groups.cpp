#include "impl.h"
#include "config.h"
#include "group_mapper.h"
#include "group_geometry_info.h"

namespace NKikimr {
    namespace NBsController {

        class TBlobStorageController::TGroupFitter {
            TConfigState& State;
            const ui32 AvailabilityDomainId;
            const bool IgnoreGroupSanityChecks;
            const bool IgnoreGroupFailModelChecks;
            const bool IgnoreDegradedGroupsChecks;
            const bool IgnoreVSlotQuotaCheck;
            const bool AllowUnusableDisks;
            const bool SettleOnlyOnOperationalDisks;
            std::deque<ui64> ExpectedSlotSize;
            const ui32 PDiskSpaceMarginPromille;
            const TGroupGeometryInfo Geometry;
            const TBoxStoragePoolId StoragePoolId;
            const TStoragePoolInfo& StoragePool;
            std::optional<TGroupMapper> Mapper;
            NKikimrBlobStorage::TConfigResponse::TStatus& Status;
            TVSlotReadyTimestampQ& VSlotReadyTimestampQ;

        public:
            TGroupFitter(TConfigState& state, ui32 availabilityDomainId, const NKikimrBlobStorage::TConfigRequest& cmd,
                    std::deque<ui64>& expectedSlotSize, ui32 pdiskSpaceMarginPromille,
                    const TBoxStoragePoolId& storagePoolId, const TStoragePoolInfo& storagePool,
                    NKikimrBlobStorage::TConfigResponse::TStatus& status, TVSlotReadyTimestampQ& vslotReadyTimestampQ)
                : State(state)
                , AvailabilityDomainId(availabilityDomainId)
                , IgnoreGroupSanityChecks(cmd.GetIgnoreGroupSanityChecks())
                , IgnoreGroupFailModelChecks(cmd.GetIgnoreGroupFailModelChecks())
                , IgnoreDegradedGroupsChecks(cmd.GetIgnoreDegradedGroupsChecks())
                , IgnoreVSlotQuotaCheck(cmd.GetIgnoreVSlotQuotaCheck())
                , AllowUnusableDisks(cmd.GetAllowUnusableDisks())
                , SettleOnlyOnOperationalDisks(cmd.GetSettleOnlyOnOperationalDisks())
                , ExpectedSlotSize(expectedSlotSize)
                , PDiskSpaceMarginPromille(pdiskSpaceMarginPromille)
                , Geometry(TBlobStorageGroupType(storagePool.ErasureSpecies), storagePool.GetGroupGeometry())
                , StoragePoolId(storagePoolId)
                , StoragePool(storagePool)
                , Status(status)
                , VSlotReadyTimestampQ(vslotReadyTimestampQ)
            {}

            void CheckReserve(ui32 total, ui32 min, ui32 part) {
                // number of reserved groups is min + part * maxGroups in cluster
                for (ui64 reserve = 0; reserve < min || (reserve - min) * 1000000 / Max<ui64>(1, total) < part; ++reserve, ++total) {
                    TGroupMapper::TGroupDefinition group;
                    try {
                        AllocateGroup(0, group, {}, {}, 0, false);
                    } catch (const TExFitGroupError&) {
                        throw TExError() << "group reserve constraint hit";
                    }
                }
            }

            void CreateGroup() {
                ////////////////////////////////////////////////////////////////////////////////////////////
                // ALLOCATE GROUP ID FOR THE NEW GROUP
                ////////////////////////////////////////////////////////////////////////////////////////////
                TGroupId groupId;
                for (;;) {
                    // obtain group local id
                    auto& nextGroupId = State.NextGroupId.Unshare();
                    const ui32 groupLocalId = nextGroupId ? TGroupID(nextGroupId).GroupLocalID() : 0;

                    // create new full group id
                    TGroupID fullGroupId(GroupConfigurationTypeDynamic, AvailabilityDomainId, groupLocalId);
                    TGroupID nextFullGroupId = fullGroupId;
                    ++nextFullGroupId;

                    // write down NextGroupId
                    nextGroupId = nextFullGroupId.GetRaw();

                    // exit if there is no collision
                    groupId = fullGroupId.GetRaw();
                    if (!State.Groups.Find(groupId)) {
                        break;
                    }
                }

                ////////////////////////////////////////////////////////////////////////////////////////////////
                // CREATE MORE GROUPS
                ////////////////////////////////////////////////////////////////////////////////////////////////
                TGroupMapper::TGroupDefinition group;
                i64 requiredSpace = Min<i64>();
                if (!ExpectedSlotSize.empty()) {
                    requiredSpace = ExpectedSlotSize.front();
                    ExpectedSlotSize.pop_front();
                }
                AllocateGroup(groupId, group, {}, {}, requiredSpace, false);

                // scan all comprising PDisks for PDiskCategory
                TMaybe<TPDiskCategory> desiredPDiskCategory;

                for (const auto& realm : group) {
                    for (const auto& domain : realm) {
                        for (const TPDiskId& disk : domain) {
                            if (const TPDiskInfo *pdisk = State.PDisks.Find(disk); pdisk && !State.PDisksToRemove.find(disk)) {
                                if (requiredSpace != Min<i64>() && !pdisk->SlotSpaceEnforced(State.Self)) {
                                    Mapper->AdjustSpaceAvailable(disk, -requiredSpace);
                                }
                                if (!desiredPDiskCategory) {
                                    desiredPDiskCategory = pdisk->Kind;
                                } else if (*desiredPDiskCategory != pdisk->Kind) {
                                    desiredPDiskCategory = 0;
                                }
                            } else {
                                throw TExFitGroupError() << "can't find PDisk# " << disk;
                            }
                        }
                    }
                }

                // create group info
                const ui64 MainKeyVersion = 0;
                ui32 lifeCyclePhase = 0;
                TString mainKeyId = "";
                TString encryptedGroupKey = "";
                ui64 groupKeyNonce = groupId; // For the first time use groupId, then use low 32 bits of the
                                              // NextGroupKeyNonce to produce high 32 bits of the groupKeyNonce.

                TGroupInfo *groupInfo = State.Groups.ConstructInplaceNewEntry(groupId, groupId, 1,
                    0, Geometry.GetErasure(), desiredPDiskCategory.GetOrElse(0), StoragePool.VDiskKind,
                    StoragePool.EncryptionMode.GetOrElse(0), lifeCyclePhase, mainKeyId, encryptedGroupKey,
                    groupKeyNonce, MainKeyVersion, false, false, StoragePoolId, Geometry.GetNumFailRealms(),
                    Geometry.GetNumFailDomainsPerFailRealm(), Geometry.GetNumVDisksPerFailDomain());

                // bind group to storage pool
                groupInfo->StoragePoolId = StoragePoolId;
                State.StoragePoolGroups.Unshare().emplace(StoragePoolId, groupId);

                const TGroupSpecies species = groupInfo->GetGroupSpecies();
                auto& index = State.IndexGroupSpeciesToGroup.Unshare();
                index[species].push_back(groupId);

                // create VSlots
                CreateVSlotsForGroup(groupInfo, group, {});
            }

            void CheckExistingGroup(TGroupId groupId) {
                ////////////////////////////////////////////////////////////////////////////////////////////////////////
                // extract TGroupInfo for specified group
                ////////////////////////////////////////////////////////////////////////////////////////////////////////
                const TGroupInfo *groupInfo = State.Groups.Find(groupId);
                if (!groupInfo) {
                    throw TExFitGroupError() << "GroupId# " << groupId << " not found";
                }

                TGroupMapper::TGroupDefinition group;

                auto getGroup = [&]() -> TGroupMapper::TGroupDefinition& {
                    if (!group) {
                        Geometry.ResizeGroup(group);
                        for (const TVSlotInfo *vslot : groupInfo->VDisksInGroup) {
                            const TPDiskId& pdiskId = vslot->VSlotId.ComprisingPDiskId();
                            group[vslot->RingIdx][vslot->FailDomainIdx][vslot->VDiskIdx] = pdiskId;
                        }
                    }

                    return group;
                };

                // a set of preserved slots (which are not changed during this transaction)
                TMap<TVDiskID, TVSlotId> preservedSlots;

                // mapping for audit log
                TMap<TVDiskIdShort, TVSlotId> replacedSlots;
                TStackVec<std::pair<TVSlotId, bool>, 32> replaceQueue;
                THashMap<TVDiskIdShort, TPDiskId> replacedDisks;
                i64 requiredSpace = Min<i64>();

                ////////////////////////////////////////////////////////////////////////////////////////////////////////
                // scan through all VSlots and find matching PDisks
                ////////////////////////////////////////////////////////////////////////////////////////////////////////
                for (const TVSlotInfo *vslot : groupInfo->VDisksInGroup) {
                    const auto it = State.ExplicitReconfigureMap.find(vslot->VSlotId);
                    bool replace = it != State.ExplicitReconfigureMap.end();
                    const TPDiskId targetPDiskId = replace ? it->second : TPDiskId();
                    if (!replace) {
                        // check status
                        switch (vslot->PDisk->Status) {
                            case NKikimrBlobStorage::EDriveStatus::ACTIVE:
                                // nothing to do, it's okay
                                break;

                            case NKikimrBlobStorage::EDriveStatus::INACTIVE:
                                // nothing to do as well, new groups are not created on this drive, but existing ones continue
                                // to function as expected
                                break;

                            case NKikimrBlobStorage::EDriveStatus::BROKEN:
                                // reconfigure group
                                replace = true;
                                break;

                            case NKikimrBlobStorage::EDriveStatus::FAULTY:
                            case NKikimrBlobStorage::EDriveStatus::TO_BE_REMOVED:
                                // groups are moved out asynchronously
                                break;

                            default:
                                Y_FAIL("unexpected drive status");
                        }
                    }

                    if (replace) {
                        auto& g = getGroup();
                        // get the current PDisk in the desired slot and replace it with the target one; if the target
                        // PDisk id is zero, then new PDisk will be picked up automatically
                        g[vslot->RingIdx][vslot->FailDomainIdx][vslot->VDiskIdx] = targetPDiskId;
                        replacedSlots.emplace(TVDiskIdShort(vslot->RingIdx, vslot->FailDomainIdx, vslot->VDiskIdx), vslot->VSlotId);
                        replaceQueue.emplace_back(vslot->VSlotId, State.SuppressDonorMode.count(vslot->VSlotId));
                        replacedDisks.emplace(vslot->GetShortVDiskId(), vslot->VSlotId.ComprisingPDiskId());
                    } else {
                        preservedSlots.emplace(vslot->GetVDiskId(), vslot->VSlotId);
                        auto& m = vslot->Metrics;
                        if (m.HasAllocatedSize() && !IgnoreVSlotQuotaCheck) {
                            // calculate space as the maximum of allocated sizes of untouched vdisks
                            requiredSpace = Max<i64>(requiredSpace, m.GetAllocatedSize());
                        }
                    }
                }

                if (group) {
                    TGroupInfo *groupInfo = State.Groups.FindForUpdate(groupId);

                    // we have something changed in the group; initiate remapping
                    bool hasMissingSlots = false;
                    bool adjustSpaceAvailable = false;
                    for (const auto& realm : group) {
                        for (const auto& domain : realm) {
                            for (const TPDiskId& pdisk : domain) {
                                if (pdisk == TPDiskId()) {
                                    hasMissingSlots = true;
                                }
                            }
                        }
                    }
                    if (hasMissingSlots || !IgnoreGroupSanityChecks) {
                        TGroupMapper::TForbiddenPDisks forbid;
                        for (const auto& vslot : groupInfo->VDisksInGroup) {
                            for (const auto& [vslotId, vdiskId] : vslot->Donors) {
                                if (group[vdiskId.FailRealm][vdiskId.FailDomain][vdiskId.VDisk] == TPDiskId()) {
                                    forbid.insert(vslotId.ComprisingPDiskId());
                                }
                            }
                        }
                        AllocateGroup(groupId, group, replacedDisks, std::move(forbid), requiredSpace, AllowUnusableDisks);
                        if (!IgnoreVSlotQuotaCheck) {
                            adjustSpaceAvailable = true;
                            for (const auto& [pos, vslotId] : replacedSlots) {
                                const TPDiskId& pdiskId = group[pos.FailRealm][pos.FailDomain][pos.VDisk];
                                const TPDiskInfo *pdisk = State.PDisks.Find(pdiskId);
                                if (!pdisk->SlotSpaceEnforced(State.Self)) {
                                    Mapper->AdjustSpaceAvailable(pdiskId, -requiredSpace);
                                }
                            }
                        }
                    }

                    // create slots for the new group
                    auto newSlots = CreateVSlotsForGroup(groupInfo, group, preservedSlots);
                    groupInfo->ContentChanged = true;

                    if (replacedSlots) {
                        if (!IgnoreGroupFailModelChecks) {
                            // process only groups with changed content; create topology for group
                            auto& topology = *groupInfo->Topology;
                            // fill in vector of failed disks (that are not fully operational)
                            TBlobStorageGroupInfo::TGroupVDisks failed(&topology);
                            for (const TVSlotInfo *slot : groupInfo->VDisksInGroup) {
                                if (!slot->IsOperational()) {
                                    failed |= {&topology, slot->GetShortVDiskId()};
                                }
                            }
                            // check the failure model
                            auto& checker = *topology.QuorumChecker;
                            if (!checker.CheckFailModelForGroup(failed)) {
                                throw TExMayLoseData(groupId);
                            } else if (!IgnoreDegradedGroupsChecks && checker.IsDegraded(failed)) {
                                throw TExMayGetDegraded(groupId);
                            }
                        }

                        for (const TVSlotInfo *slot : groupInfo->VDisksInGroup) {
                            const TVDiskIdShort pos(slot->RingIdx, slot->FailDomainIdx, slot->VDiskIdx);
                            if (const auto it = replacedSlots.find(pos); it != replacedSlots.end()) {
                                auto *item = Status.AddReassignedItem();
                                VDiskIDFromVDiskID(TVDiskID(groupInfo->ID, groupInfo->Generation - 1, pos), item->MutableVDiskId());
                                Serialize(item->MutableFrom(), it->second);
                                Serialize(item->MutableTo(), slot->VSlotId);
                                if (auto *pdisk = State.PDisks.Find(it->second.ComprisingPDiskId())) {
                                    item->SetFromFqdn(std::get<0>(pdisk->HostId));
                                    item->SetFromPath(pdisk->Path);
                                }
                                if (auto *pdisk = State.PDisks.Find(slot->VSlotId.ComprisingPDiskId())) {
                                    item->SetToFqdn(std::get<0>(pdisk->HostId));
                                    item->SetToPath(pdisk->Path);
                                }
                            }
                        }

                        auto makeReplacements = [&] {
                            TStringBuilder s;
                            s << "[";
                            bool first = true;
                            for (const auto& kv : replacedSlots) {
                                s << (std::exchange(first, false) ? "" : " ")
                                    << "{" << kv.first << " from# " << kv.second << " to# "
                                    << group[kv.first.FailRealm][kv.first.FailDomain][kv.first.VDisk] << "}";
                            }
                            return static_cast<TString>(s << "]");
                        };
                        STLOG(PRI_INFO, BS_CONTROLLER_AUDIT, BSCA04, "ReconfigGroup", (UniqueId, State.UniqueId),
                            (GroupId, groupInfo->ID),
                            (GroupGeneration, groupInfo->Generation),
                            (Replacements, makeReplacements()));
                    }

                    for (const auto& [vslotId, suppressDonorMode] : replaceQueue) {
                        if (State.DonorMode && !suppressDonorMode && !State.UncommittedVSlots.count(vslotId)) {
                            TVSlotInfo *mutableSlot = State.VSlots.FindForUpdate(vslotId);
                            Y_VERIFY(mutableSlot);
                            // make slot the donor one for the newly created slot
                            const auto it = newSlots.find(mutableSlot->GetShortVDiskId());
                            Y_VERIFY(it != newSlots.end());
                            mutableSlot->MakeDonorFor(it->second);
                            for (const auto& [donorVSlotId, donorVDiskId] : it->second->Donors) {
                                TVSlotInfo *mutableDonor = State.VSlots.FindForUpdate(donorVSlotId);
                                Y_VERIFY(mutableDonor);
                                Y_VERIFY(mutableDonor->Mood == TMood::Donor);
                                mutableDonor->AcceptorVSlotId = it->second->VSlotId;
                            }
                        } else {
                            if (adjustSpaceAvailable) {
                                const TVSlotInfo *slot = State.VSlots.Find(vslotId);
                                Y_VERIFY(slot);
                                if (!slot->PDisk->SlotSpaceEnforced(State.Self)) {
                                    // mark the space from destroyed slot as available
                                    Mapper->AdjustSpaceAvailable(vslotId.ComprisingPDiskId(), slot->Metrics.GetAllocatedSize());
                                }
                            }

                            State.DestroyVSlot(vslotId);
                        }
                    }
                } else {
                    Y_VERIFY(replaceQueue.empty());
                }
            }

        private:
            void AllocateGroup(TGroupId groupId, TGroupMapper::TGroupDefinition& group,
                    const THashMap<TVDiskIdShort, TPDiskId>& replacedDisks, TGroupMapper::TForbiddenPDisks forbid,
                    i64 requiredSpace, bool addExistingDisks) {
                if (!Mapper) {
                    Mapper.emplace(Geometry, StoragePool.RandomizeGroupMapping);
                    PopulateGroupMapper();
                }
                TStackVec<TPDiskId, 32> removeQ;
                if (addExistingDisks) {
                    for (const auto& realm : group) {
                        for (const auto& domain : realm) {
                            for (const TPDiskId id : domain) {
                                if (id != TPDiskId()) {
                                    if (auto *info = State.PDisks.Find(id); info && RegisterPDisk(id, *info, false)) {
                                        removeQ.push_back(id);
                                    }
                                }
                            }
                        }
                    }
                }
                Geometry.AllocateGroup(*Mapper, groupId, group, replacedDisks, std::move(forbid), requiredSpace);
                for (const TPDiskId pdiskId : removeQ) {
                    Mapper->UnregisterPDisk(pdiskId);
                }
            }

            void PopulateGroupMapper() {
                const TBoxId boxId = std::get<0>(StoragePoolId);

                State.PDisks.ForEach([&](const TPDiskId& id, const TPDiskInfo& info) {
                    if (info.BoxId != boxId) {
                        return; // ignore disks not from desired box
                    }

                    if (State.PDisksToRemove.count(id)) {
                        return; // this PDisk is scheduled for removal
                    }

                    for (const auto& filter : StoragePool.PDiskFilters) {
                        if (filter.MatchPDisk(info)) {
                            const bool inserted = RegisterPDisk(id, info, true);
                            Y_VERIFY(inserted);
                            break;
                        }
                    }
                });
            }

            bool RegisterPDisk(TPDiskId id, const TPDiskInfo& info, bool usable) {
                // calculate number of used slots on this PDisk, also counting the static ones
                ui32 numSlots = info.NumActiveSlots + info.StaticSlotUsage;

                // create a set of groups residing on this PDisk
                TStackVec<ui32, 16> groups;
                for (const auto& [vslotId, vslot] : info.VSlotsOnPDisk) {
                    if (!vslot->IsBeingDeleted()) {
                        groups.push_back(vslot->GroupId);
                    }
                }

                // calculate vdisk space quota (or amount of available space when no enforcement is enabled)
                i64 availableSpace = Max<i64>();
                if (usable && !IgnoreVSlotQuotaCheck) {
                    if (info.SlotSpaceEnforced(State.Self)) {
                        availableSpace = info.Metrics.GetEnforcedDynamicSlotSize() * (1000 - PDiskSpaceMarginPromille) / 1000;
                    } else {
                        // here we assume that no space enforcement takes place and we have to calculate available space
                        // for this disk; we take it as available space and keep in mind that PDisk must have at least
                        // PDiskSpaceMarginPromille space remaining
                        availableSpace = info.Metrics.GetAvailableSize() - info.Metrics.GetTotalSize() * PDiskSpaceMarginPromille / 1000;

                        // also we have to find replicating VSlots on this PDisk and assume they consume up to
                        // max(vslotSize for every slot in group), not their actual AllocatedSize
                        for (const auto& [id, slot] : info.VSlotsOnPDisk) {
                            if (slot->Group && slot->GetStatus() != NKikimrBlobStorage::EVDiskStatus::READY) {
                                ui64 maxGroupSlotSize = 0;
                                for (const TVSlotInfo *peer : slot->Group->VDisksInGroup) {
                                    maxGroupSlotSize = Max(maxGroupSlotSize, peer->Metrics.GetAllocatedSize());
                                }
                                // return actually used space to available pool
                                availableSpace += slot->Metrics.GetAllocatedSize();
                                // and consume expected slot size after replication finishes
                                availableSpace -= maxGroupSlotSize;
                            }
                        }
                    }
                }

                if (!info.AcceptsNewSlots()) {
                    usable = false;
                }

                if (SettleOnlyOnOperationalDisks && !info.Operational) {
                    usable = false;
                }

                // register PDisk in the mapper
                return Mapper->RegisterPDisk({
                    .PDiskId = id,
                    .Location = State.HostRecords->GetLocation(id.NodeId),
                    .Usable = usable,
                    .NumSlots = numSlots,
                    .MaxSlots = info.ExpectedSlotCount,
                    .Groups = std::move(groups),
                    .SpaceAvailable = availableSpace,
                    .Operational = info.Operational,
                    .Decommitted = info.Decommitted(),
                });
            }

            std::map<TVDiskIdShort, TVSlotInfo*> CreateVSlotsForGroup(TGroupInfo *groupInfo,
                    const TGroupMapper::TGroupDefinition& group, const TMap<TVDiskID, TVSlotId>& preservedSlots) {
                std::map<TVDiskIdShort, TVSlotInfo*> res;

                // reset group contents as we are going to fill it right now
                groupInfo->ClearVDisksInGroup();

                for (ui32 failRealmIdx = 0; failRealmIdx < group.size(); ++failRealmIdx) {
                    const auto& realm = group[failRealmIdx];
                    for (ui32 failDomainIdx = 0; failDomainIdx < realm.size(); ++failDomainIdx) {
                        const auto& domain = realm[failDomainIdx];
                        for (ui32 vdiskIdx = 0; vdiskIdx < domain.size(); ++vdiskIdx) {
                            const TVDiskID vdiskId(groupInfo->ID, groupInfo->Generation, failRealmIdx, failDomainIdx, vdiskIdx);
                            if (auto it = preservedSlots.find(vdiskId); it != preservedSlots.end()) {
                                const TVSlotInfo *vslotInfo = State.VSlots.Find(it->second);
                                Y_VERIFY(vslotInfo);
                                groupInfo->AddVSlot(vslotInfo);
                            } else {
                                const TPDiskId pdiskId = domain[vdiskIdx];
                                Y_VERIFY(!State.PDisksToRemove.count(pdiskId));
                                TPDiskInfo *pdiskInfo = State.PDisks.FindForUpdate(pdiskId);
                                Y_VERIFY(pdiskInfo);
                                TVSlotId vslotId;

                                // allocate new VSlot id; avoid collisions
                                for (;;) {
                                    const auto currentVSlotId = pdiskInfo->NextVSlotId;
                                    pdiskInfo->NextVSlotId = currentVSlotId + 1;
                                    vslotId = TVSlotId(pdiskId, currentVSlotId);
                                    if (!State.VSlots.Find(vslotId)) {
                                        break;
                                    }
                                }

                                // insert new VSlot
                                TVSlotInfo *vslotInfo = State.VSlots.ConstructInplaceNewEntry(vslotId, vslotId, pdiskInfo,
                                    groupInfo->ID, 0, groupInfo->Generation, StoragePool.VDiskKind, failRealmIdx,
                                    failDomainIdx, vdiskIdx, TMood::Normal, groupInfo, &VSlotReadyTimestampQ,
                                    TInstant::Zero());

                                // mark as uncommitted
                                State.UncommittedVSlots.insert(vslotId);

                                // remember newly created slot
                                res.emplace(vdiskId, vslotInfo);
                            }
                        }
                    }
                }

                groupInfo->FinishVDisksInGroup();
                groupInfo->CalculateGroupStatus();

                return res;
            }
        };

        void TBlobStorageController::FitGroupsForUserConfig(TConfigState& state, ui32 availabilityDomainId,
                const NKikimrBlobStorage::TConfigRequest& cmd, std::deque<ui64> expectedSlotSize,
                NKikimrBlobStorage::TConfigResponse::TStatus& status) {
            std::unordered_map<TString, std::pair<ui32, TBoxStoragePoolId>> filterMap;
            std::unordered_set<TString> changedFilters;

            // scan through all storage pools and fit the number of groups to desired one
            for (const auto& [storagePoolId, storagePool] : state.StoragePools.Get()) {
                TGroupFitter fitter(state, availabilityDomainId, cmd, expectedSlotSize, PDiskSpaceMarginPromille,
                    storagePoolId, storagePool, status, VSlotReadyTimestampQ);

                const auto& storagePoolGroups = state.StoragePoolGroups.Get();
                const auto range = storagePoolGroups.equal_range(storagePoolId);
                ui32 numActualGroups = 0;

                try {
                    TStringBuilder identifier;
                    identifier << "Erasure# " << storagePool.ErasureSpecies
                        << " Geometry# " << storagePool.RealmLevelBegin << "," << storagePool.RealmLevelEnd
                        << "," << storagePool.DomainLevelBegin << "," << storagePool.DomainLevelEnd
                        << "," << storagePool.NumFailRealms << "," << storagePool.NumFailDomainsPerFailRealm
                        << "," << storagePool.NumVDisksPerFailDomain;
                    for (const auto& filter : storagePool.PDiskFilters) {
                        identifier << " Filter# " << filter.Type << "," << filter.SharedWithOs
                            << "," << filter.ReadCentric << "," << filter.Kind;
                    }
                    auto& [numGroups, id] = filterMap[identifier];
                    numGroups += storagePool.NumGroups;
                    id = storagePoolId;

                    for (auto it = range.first; it != range.second; ++it, ++numActualGroups) {
                        fitter.CheckExistingGroup(it->second);
                    }
                    if (numActualGroups < storagePool.NumGroups) {
                        changedFilters.insert(identifier);
                    }
                    for (; numActualGroups < storagePool.NumGroups; ++numActualGroups) {
                        fitter.CreateGroup();
                    }
                } catch (const TExFitGroupError& ex) {
                    throw TExError() << "Group fit error"
                        << " BoxId# " << std::get<0>(storagePoolId)
                        << " StoragePoolId# " << std::get<1>(storagePoolId)
                        << " Error# " << ex.what();
                }
                if (storagePool.NumGroups < numActualGroups) {
                    throw TExError() << "Storage pool modification error"
                        << " BoxId# " << std::get<0>(storagePoolId)
                        << " StoragePoolId# " << std::get<1>(storagePoolId)
                        << " impossible to reduce number of groups";
                }
            }

            if (!cmd.GetIgnoreGroupReserve()) {
                for (const auto& identifier : changedFilters) {
                    auto& [numGroups, storagePoolId] = filterMap.at(identifier);
                    const auto& storagePool = state.StoragePools.Get().at(storagePoolId);
                    TGroupFitter fitter(state, availabilityDomainId, cmd, expectedSlotSize, PDiskSpaceMarginPromille,
                        storagePoolId, storagePool, status, VSlotReadyTimestampQ);
                    fitter.CheckReserve(numGroups, GroupReserveMin, GroupReservePart);
                }
            }
        }

    } // NBsController
} // NKikimr
