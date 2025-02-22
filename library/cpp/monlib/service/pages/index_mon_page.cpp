#include "index_mon_page.h"

#include <util/generic/cast.h>
#include <util/string/ascii.h>

using namespace NMonitoring;

void TIndexMonPage::OutputIndexPage(IMonHttpRequest& request) {
    request.Output() << HTTPOKHTML;
    request.Output() << "<html>\n";
    OutputHead(request.Output());
    OutputBody(request);
    request.Output() << "</html>\n";
}

void TIndexMonPage::Output(IMonHttpRequest& request) {
    TStringBuf pathInfo = request.GetPathInfo();
    if (pathInfo.empty() || pathInfo == TStringBuf("/")) {
        OutputIndexPage(request);
        return;
    }

    Y_VERIFY(pathInfo.StartsWith('/'));

    TMonPagePtr found;
    // analogous to CGI PATH_INFO
    {
        TGuard<TMutex> g(Mtx);
        TStringBuf pathTmp = request.GetPathInfo();
        for (;;) {
            TPagesByPath::iterator i = PagesByPath.find(pathTmp);
            if (i != PagesByPath.end()) {
                found = i->second;
                pathInfo = request.GetPathInfo().substr(pathTmp.size());
                Y_VERIFY(pathInfo.empty() || pathInfo.StartsWith('/'));
                break;
            }
            size_t slash = pathTmp.find_last_of('/');
            Y_VERIFY(slash != TString::npos);
            pathTmp = pathTmp.substr(0, slash);
            if (!pathTmp) {
                break;
            }
        }
    }
    if (found) {
        THolder<IMonHttpRequest> child(request.MakeChild(found.Get(), TString{pathInfo}));
        found->Output(*child);
    } else {
        request.Output() << HTTPNOTFOUND;
    }
}

void TIndexMonPage::OutputIndex(IOutputStream& out, bool pathEndsWithSlash) {
    TGuard<TMutex> g(Mtx);
    for (auto& Page : Pages) {
        IMonPage* page = Page.Get();
        if (page->IsInIndex()) {
            TString pathToDir = "";
            if (!pathEndsWithSlash) {
                pathToDir = this->GetPath() + "/";
            }
            out << "<a href='" << pathToDir << page->GetPath() << "'>" << page->GetTitle() << "</a><br/>\n";
        }
    }
}

void TIndexMonPage::Register(TMonPagePtr page) {
    TGuard<TMutex> g(Mtx);
    auto insres = PagesByPath.insert(std::make_pair("/" + page->GetPath(), page));
    if (insres.second) {
        // new unique page just inserted, update Pages
        Pages.push_back(page);
    } else {
        // a page with the given path is already present, replace it with the new page

        // find old page, sorry for O(n)
        auto it = std::find(Pages.begin(), Pages.end(), insres.first->second);
        *it = page;
        // this already present, replace it
        insres.first->second = page;
    }
    page->Parent = this;
}

TIndexMonPage* TIndexMonPage::RegisterIndexPage(const TString& path, const TString& title) {
    TGuard<TMutex> g(Mtx);
    TIndexMonPage* page = VerifyDynamicCast<TIndexMonPage*>(FindPage(path));
    if (page) {
        return page;
    }
    page = new TIndexMonPage(path, title);
    Register(page);
    return VerifyDynamicCast<TIndexMonPage*>(page);
}

IMonPage* TIndexMonPage::FindPage(const TString& relativePath) {
    TGuard<TMutex> g(Mtx);

    Y_VERIFY(!relativePath.StartsWith('/'));
    TPagesByPath::iterator i = PagesByPath.find("/" + relativePath);
    if (i == PagesByPath.end()) {
        return nullptr;
    } else {
        return i->second.Get();
    }
}

TIndexMonPage* TIndexMonPage::FindIndexPage(const TString& relativePath) {
    return VerifyDynamicCast<TIndexMonPage*>(FindPage(relativePath));
}

void TIndexMonPage::OutputCommonJsCss(IOutputStream& out) {
    out << "<link rel='stylesheet' href='https://yastatic.net/bootstrap/3.3.1/css/bootstrap.min.css'>\n";
    out << "<script language='javascript' type='text/javascript' src='https://yastatic.net/jquery/2.1.3/jquery.min.js'></script>\n";
    out << "<script language='javascript' type='text/javascript' src='https://yastatic.net/bootstrap/3.3.1/js/bootstrap.min.js'></script>\n";
}

void TIndexMonPage::OutputHead(IOutputStream& out) {
    out << "<head>\n";
    OutputCommonJsCss(out);
    out << "<title>" << Title << "</title>\n";
    out << "</head>\n";
}

void TIndexMonPage::OutputBody(IMonHttpRequest& req) {
    auto& out = req.Output();
    out << "<body>\n";

    // part of common navbar
    OutputNavBar(out);

    out << "<div class='container'>\n"
             << "<h2>" << Title << "</h2>\n";
    OutputIndex(out, req.GetPathInfo().EndsWith('/'));
    out << "</div>\n"
        << "</body>\n";
}

void TIndexMonPage::SortPages() {
    TGuard<TMutex> g(Mtx);
    std::sort(Pages.begin(), Pages.end(), [](const TMonPagePtr& a, const TMonPagePtr& b) {
        return AsciiCompareIgnoreCase(a->GetTitle(), b->GetTitle()) < 0;
    });
}

void TIndexMonPage::ClearPages() {
    TGuard<TMutex> g(Mtx);
    Pages.clear();
    PagesByPath.clear();
}
