#pragma once

#include "kqp_tasks_graph.h"

#include <ydb/library/yql/dq/common/dq_value.h>
#include <ydb/library/yql/dq/proto/dq_tasks.pb.h>
#include <ydb/library/yql/dq/proto/dq_transport.pb.h>

#include <ydb/library/yql/minikql/computation/mkql_computation_node_holders.h>

#include <util/generic/variant.h>

namespace NKikimr::NKqp {

struct TShardInfo {
    TMap<TString, NYql::NDqProto::TData> Params;
    TMap<TString, NKikimr::NMiniKQL::TType*> ParamTypes;

    TMaybe<TShardKeyRanges> KeyReadRanges;  // empty -> no reads
    TMaybe<TShardKeyRanges> KeyWriteRanges; // empty -> no writes

    TString ToString(const TVector<NScheme::TTypeId>& keyTypes, const NScheme::TTypeRegistry& typeRegistry) const;
};

TSerializedTableRange MakeKeyRange(const TVector<NUdf::TDataTypeId>& keyColumnTypes,
    const NKqpProto::TKqpPhyKeyRange& range, const TStageInfo& stageInfo, const NMiniKQL::THolderFactory& holderFactory,
    const NMiniKQL::TTypeEnvironment& typeEnv);

TVector<TSerializedPointOrRange> FillReadRanges(const TVector<NUdf::TDataTypeId>& keyColumnTypes,
    const NKqpProto::TKqpPhyOpReadOlapRanges& readRange, const TStageInfo& stageInfo,
    const NMiniKQL::THolderFactory& holderFactory, const NMiniKQL::TTypeEnvironment& typeEnv);

TVector<TSerializedPointOrRange> FillReadRanges(const TVector<NUdf::TDataTypeId>& keyColumnTypes,
    const NKqpProto::TKqpPhyOpReadRanges& readRange, const TStageInfo& stageInfo,
    const NMiniKQL::THolderFactory& holderFactory, const NMiniKQL::TTypeEnvironment& typeEnv);

THashMap<ui64, TShardInfo> PrunePartitions(const TKqpTableKeys& tableKeys,
    const NKqpProto::TKqpPhyOpReadRange& readRange, const TStageInfo& stageInfo,
    const NMiniKQL::THolderFactory& holderFactory, const NMiniKQL::TTypeEnvironment& typeEnv);

THashMap<ui64, TShardInfo> PrunePartitions(const TKqpTableKeys& tableKeys,
    const NKqpProto::TKqpPhyOpReadRanges& readRanges, const TStageInfo& stageInfo,
    const NMiniKQL::THolderFactory& holderFactory, const NMiniKQL::TTypeEnvironment& typeEnv);

THashMap<ui64, TShardInfo> PrunePartitions(const TKqpTableKeys& tableKeys,
    const NKqpProto::TKqpPhyOpLookup& lookup, const TStageInfo& stageInfo,
    const NMiniKQL::THolderFactory& holderFactory, const NMiniKQL::TTypeEnvironment& typeEnv);

THashMap<ui64, TShardInfo> PruneEffectPartitions(const TKqpTableKeys& tableKeys,
    const NKqpProto::TKqpPhyOpUpsertRows& effect, const TStageInfo& stageInfo,
    const NMiniKQL::THolderFactory& holderFactory, const NMiniKQL::TTypeEnvironment& typeEnv);

THashMap<ui64, TShardInfo> PruneEffectPartitions(const TKqpTableKeys& tableKeys,
    const NKqpProto::TKqpPhyOpDeleteRows& effect, const TStageInfo& stageInfo,
    const NMiniKQL::THolderFactory& holderFactory, const NMiniKQL::TTypeEnvironment& typeEnv);

} // namespace NKikimr::NKqp
