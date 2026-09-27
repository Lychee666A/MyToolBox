#include "EverythingApi.h"

#include <QDir>
#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QMutexLocker>

#include <windows.h>

// Everything SDK 头文件（third_party/everything/include/Everything.h）
#include "Everything.h"

// ============================================================
//                        函数指针
// ============================================================
typedef void  (__stdcall *fn_SetSearchW)(LPCWSTR);
typedef void  (__stdcall *fn_SetMatchCase)(BOOL);
typedef void  (__stdcall *fn_SetMatchWholeWord)(BOOL);
typedef void  (__stdcall *fn_SetMatchPath)(BOOL);
typedef void  (__stdcall *fn_SetRegex)(BOOL);
typedef void  (__stdcall *fn_SetMax)(DWORD);
typedef void  (__stdcall *fn_SetOffset)(DWORD);
typedef void  (__stdcall *fn_SetRequestFlags)(DWORD);
typedef void  (__stdcall *fn_SetSort)(DWORD);
typedef BOOL  (__stdcall *fn_QueryW)(BOOL);
typedef DWORD (__stdcall *fn_GetNumResults)();
typedef BOOL  (__stdcall *fn_IsFolderResult)(DWORD);
typedef BOOL  (__stdcall *fn_IsFileResult)(DWORD);
typedef void  (__stdcall *fn_GetResultFullPathNameW)(DWORD, LPWSTR, DWORD);
typedef BOOL  (__stdcall *fn_GetResultSize)(DWORD, LARGE_INTEGER*);
typedef BOOL  (__stdcall *fn_GetResultDateModified)(DWORD, FILETIME*);
typedef BOOL  (__stdcall *fn_GetResultDateCreated)(DWORD, FILETIME*);
typedef BOOL  (__stdcall *fn_GetResultDateAccessed)(DWORD, FILETIME*);
typedef BOOL  (__stdcall *fn_GetResultDateRecentlyChanged)(DWORD, FILETIME*);
typedef BOOL  (__stdcall *fn_GetResultAttributes)(DWORD, DWORD*);
typedef DWORD (__stdcall *fn_GetResultRunCount)(DWORD);
typedef void  (__stdcall *fn_GetResultExtensionW)(DWORD, LPWSTR, DWORD);
typedef DWORD (__stdcall *fn_GetTotResults)();
typedef DWORD (__stdcall *fn_GetTotFileResults)();
typedef DWORD (__stdcall *fn_GetTotFolderResults)();
typedef DWORD (__stdcall *fn_GetLastError)();
typedef void  (__stdcall *fn_Reset)();
typedef BOOL  (__stdcall *fn_IsDBLoaded)();

static fn_SetSearchW                   pSetSearchW                   = nullptr;
static fn_SetMatchCase                 pSetMatchCase                 = nullptr;
static fn_SetMatchWholeWord            pSetMatchWholeWord            = nullptr;
static fn_SetMatchPath                 pSetMatchPath                 = nullptr;
static fn_SetRegex                     pSetRegex                     = nullptr;
static fn_SetMax                       pSetMax                       = nullptr;
static fn_SetOffset                    pSetOffset                    = nullptr;
static fn_SetRequestFlags              pSetRequestFlags              = nullptr;
static fn_SetSort                      pSetSort                      = nullptr;
static fn_QueryW                       pQueryW                       = nullptr;
static fn_GetNumResults                pGetNumResults                = nullptr;
static fn_IsFolderResult               pIsFolderResult               = nullptr;
static fn_IsFileResult                 pIsFileResult                 = nullptr;
static fn_GetResultFullPathNameW       pGetResultFullPathNameW       = nullptr;
static fn_GetResultSize                pGetResultSize                = nullptr;
static fn_GetResultDateModified        pGetResultDateModified        = nullptr;
static fn_GetResultDateCreated         pGetResultDateCreated         = nullptr;
static fn_GetResultDateAccessed        pGetResultDateAccessed        = nullptr;
static fn_GetResultDateRecentlyChanged pGetResultDateRecentlyChanged = nullptr;
static fn_GetResultAttributes          pGetResultAttributes          = nullptr;
static fn_GetResultRunCount            pGetResultRunCount            = nullptr;
static fn_GetResultExtensionW          pGetResultExtensionW          = nullptr;
static fn_GetTotResults                pGetTotResults                = nullptr;
static fn_GetTotFileResults            pGetTotFileResults            = nullptr;
static fn_GetTotFolderResults          pGetTotFolderResults          = nullptr;
static fn_GetLastError                 pGetLastError                 = nullptr;
static fn_Reset                        pReset                        = nullptr;
static fn_IsDBLoaded                   pIsDBLoaded                   = nullptr;

// ============================================================
//                        单例
// ============================================================
EverythingApi &EverythingApi::instance()
{
	static EverythingApi s;
	return s;
}

EverythingApi::EverythingApi()
{
	m_available = loadDll();
}

EverythingApi::~EverythingApi()
{
	if (m_dll) {
		::FreeLibrary(reinterpret_cast<HMODULE>(m_dll));
		m_dll = nullptr;
	}
}

// ============================================================
//                        加载 DLL
// ============================================================
bool EverythingApi::loadDll()
{
	if (m_dll) return true;
	
	const QString dllPath = QDir(QCoreApplication::applicationDirPath())
	.filePath("Everything64.dll");
	
	if (!QFileInfo::exists(dllPath)) {
		m_lastError = QStringLiteral("找不到 Everything64.dll：%1").arg(dllPath);
		qWarning() << m_lastError;
		return false;
	}
	
	HMODULE h = ::LoadLibraryW(reinterpret_cast<LPCWSTR>(dllPath.utf16()));
	if (!h) {
		m_lastError = QStringLiteral("LoadLibrary 失败，GetLastError = %1")
		.arg(::GetLastError());
		qWarning() << m_lastError;
		return false;
	}
	
	m_dll = h;
	resolveFunctions();
	
	if (!pQueryW || !pSetSearchW) {
		m_lastError = QStringLiteral("Everything64.dll 缺少关键导出函数");
		qWarning() << m_lastError;
		::FreeLibrary(h);
		m_dll = nullptr;
		return false;
	}
	
	qDebug() << "Everything64.dll loaded OK";
	return true;
}

void EverythingApi::resolveFunctions()
{
	HMODULE h = reinterpret_cast<HMODULE>(m_dll);
	
#define RESOLVE(fnPtr, nameStr) \
	fnPtr = reinterpret_cast<decltype(fnPtr)>(::GetProcAddress(h, nameStr)); \
	if (!fnPtr) qWarning() << "Missing export:" << nameStr;
	
	RESOLVE(pSetSearchW,                   "Everything_SetSearchW")
	RESOLVE(pSetMatchCase,                 "Everything_SetMatchCase")
	RESOLVE(pSetMatchWholeWord,            "Everything_SetMatchWholeWord")
	RESOLVE(pSetMatchPath,                 "Everything_SetMatchPath")
	RESOLVE(pSetRegex,                     "Everything_SetRegex")
	RESOLVE(pSetMax,                       "Everything_SetMax")
	RESOLVE(pSetOffset,                    "Everything_SetOffset")
	RESOLVE(pSetRequestFlags,              "Everything_SetRequestFlags")
	RESOLVE(pSetSort,                      "Everything_SetSort")
	RESOLVE(pQueryW,                       "Everything_QueryW")
	RESOLVE(pGetNumResults,                "Everything_GetNumResults")
	RESOLVE(pIsFolderResult,               "Everything_IsFolderResult")
	RESOLVE(pIsFileResult,                 "Everything_IsFileResult")
	RESOLVE(pGetResultFullPathNameW,       "Everything_GetResultFullPathNameW")
	RESOLVE(pGetResultSize,                "Everything_GetResultSize")
	RESOLVE(pGetResultDateModified,        "Everything_GetResultDateModified")
	RESOLVE(pGetResultDateCreated,         "Everything_GetResultDateCreated")
	RESOLVE(pGetResultDateAccessed,        "Everything_GetResultDateAccessed")
	RESOLVE(pGetResultDateRecentlyChanged, "Everything_GetResultDateRecentlyChanged")
	RESOLVE(pGetResultAttributes,          "Everything_GetResultAttributes")
	RESOLVE(pGetResultRunCount,            "Everything_GetResultRunCount")
	RESOLVE(pGetResultExtensionW,          "Everything_GetResultExtensionW")
	RESOLVE(pGetTotResults,                "Everything_GetTotResults")
	RESOLVE(pGetTotFileResults,            "Everything_GetTotFileResults")
	RESOLVE(pGetTotFolderResults,          "Everything_GetTotFolderResults")
	RESOLVE(pGetLastError,                 "Everything_GetLastError")
	RESOLVE(pReset,                        "Everything_Reset")
	RESOLVE(pIsDBLoaded,                   "Everything_IsDBLoaded")
	
#undef RESOLVE
}

// ============================================================
//                        运行状态
// ============================================================
bool EverythingApi::isEverythingRunning() const
{
	HWND hwnd = ::FindWindowW(L"EVERYTHING_TASKBAR_NOTIFICATION", nullptr);
	return hwnd != nullptr;
}

bool EverythingApi::isDbLoaded() const
{
	if (!m_available || !pIsDBLoaded) return false;
	if (!isEverythingRunning())       return false;
	return pIsDBLoaded() != FALSE;
}

// ============================================================
//                        搜索
// ============================================================
QList<EverythingResult> EverythingApi::search(const QString &query,
											  const EverythingQueryOptions &opt,
											  EverythingStats *statsOut)
{
	QMutexLocker locker(&m_mutex);
	QList<EverythingResult> results;
	
	if (!m_available) {
		m_lastError = QStringLiteral("Everything64.dll 未加载");
		return results;
	}
	if (!isEverythingRunning()) {
		m_lastError = QStringLiteral("Everything.exe 未运行");
		return results;
	}
	
	// ★ 拼查询串
	QString q = query;
	if (opt.matchCase)      q.prepend(QStringLiteral("case:"));
	if (opt.matchWholeWord) q.prepend(QStringLiteral("ww:"));
	if (opt.matchMatchPath) q.prepend(QStringLiteral("path:"));
	if (opt.regex)          q.prepend(QStringLiteral("regex:"));
	if (opt.filesOnly)      q += QStringLiteral(" /a-d");
	if (opt.foldersOnly)    q += QStringLiteral(" /ad");
	
	const std::wstring wq = q.toStdWString();
	
	if (pReset) pReset();
	pSetSearchW(wq.c_str());
	pSetMatchCase(opt.matchCase ? TRUE : FALSE);
	pSetMatchWholeWord(opt.matchWholeWord ? TRUE : FALSE);
	pSetMatchPath(opt.matchMatchPath ? TRUE : FALSE);
	pSetRegex(opt.regex ? TRUE : FALSE);
	pSetMax(opt.maxResults);
	pSetOffset(opt.offset);
	
	// ★ 排序
	DWORD sortType = EVERYTHING_SORT_NAME_ASCENDING;
	switch (opt.sortBy) {
		case EverythingSortBy::Path:                 sortType = opt.sortAscending ? EVERYTHING_SORT_PATH_ASCENDING : EVERYTHING_SORT_PATH_DESCENDING; break;
		case EverythingSortBy::Size:                 sortType = opt.sortAscending ? EVERYTHING_SORT_SIZE_ASCENDING : EVERYTHING_SORT_SIZE_DESCENDING; break;
		case EverythingSortBy::Extension:            sortType = opt.sortAscending ? EVERYTHING_SORT_EXTENSION_ASCENDING : EVERYTHING_SORT_EXTENSION_DESCENDING; break;
		case EverythingSortBy::DateModified:         sortType = opt.sortAscending ? EVERYTHING_SORT_DATE_MODIFIED_ASCENDING : EVERYTHING_SORT_DATE_MODIFIED_DESCENDING; break;
		case EverythingSortBy::DateCreated:          sortType = opt.sortAscending ? EVERYTHING_SORT_DATE_CREATED_ASCENDING : EVERYTHING_SORT_DATE_CREATED_DESCENDING; break;
		case EverythingSortBy::DateAccessed:         sortType = opt.sortAscending ? EVERYTHING_SORT_DATE_ACCESSED_ASCENDING : EVERYTHING_SORT_DATE_ACCESSED_DESCENDING; break;
		case EverythingSortBy::Attributes:           sortType = opt.sortAscending ? EVERYTHING_SORT_ATTRIBUTES_ASCENDING : EVERYTHING_SORT_ATTRIBUTES_DESCENDING; break;
		case EverythingSortBy::RunCount:             sortType = opt.sortAscending ? EVERYTHING_SORT_RUN_COUNT_ASCENDING : EVERYTHING_SORT_RUN_COUNT_DESCENDING; break;
		case EverythingSortBy::DateRecentlyChanged:  sortType = opt.sortAscending ? EVERYTHING_SORT_DATE_RECENTLY_CHANGED_ASCENDING : EVERYTHING_SORT_DATE_RECENTLY_CHANGED_DESCENDING; break;
		default:                                     sortType = opt.sortAscending ? EVERYTHING_SORT_NAME_ASCENDING : EVERYTHING_SORT_NAME_DESCENDING; break;
	}
	pSetSort(sortType);
	
	// 请求所有字段
	pSetRequestFlags(
					 EVERYTHING_REQUEST_FULL_PATH_AND_FILE_NAME |
					 EVERYTHING_REQUEST_SIZE |
					 EVERYTHING_REQUEST_DATE_MODIFIED |
					 EVERYTHING_REQUEST_DATE_CREATED |
					 EVERYTHING_REQUEST_DATE_ACCESSED |
					 EVERYTHING_REQUEST_ATTRIBUTES |
					 EVERYTHING_REQUEST_RUN_COUNT |
					 EVERYTHING_REQUEST_EXTENSION
					 );
	
	if (!pQueryW(TRUE)) {
		const DWORD err = pGetLastError ? pGetLastError() : 0;
		m_lastError = QStringLiteral("Everything_Query 失败，错误码 = %1").arg(err);
		qWarning() << m_lastError;
		return results;
	}
	
	const DWORD n = pGetNumResults();
	results.reserve(int(n));
	
	auto ft2dt = [](const FILETIME &ft) -> QDateTime {
		ULARGE_INTEGER u;
		u.LowPart  = ft.dwLowDateTime;
		u.HighPart = ft.dwHighDateTime;
		const qint64 ms = qint64(u.QuadPart / 10000ULL) - 11644473600000LL;
		return QDateTime::fromMSecsSinceEpoch(ms);
	};
	
	for (DWORD i = 0; i < n; ++i) {
		EverythingResult r;
		
		// 完整路径
		wchar_t buf[MAX_PATH * 2] = {};
		pGetResultFullPathNameW(i, buf, MAX_PATH * 2);
		r.fullPath = QString::fromWCharArray(buf);
		
		// 大小
		LARGE_INTEGER sz{};
		if (pGetResultSize(i, &sz)) r.size = quint64(sz.QuadPart);
		
		// 时间
		FILETIME ft{};
		if (pGetResultDateModified(i, &ft))        r.modified            = ft2dt(ft);
		if (pGetResultDateCreated(i, &ft))         r.dateCreated         = ft2dt(ft);
		if (pGetResultDateAccessed(i, &ft))        r.dateAccessed        = ft2dt(ft);
		if (pGetResultDateRecentlyChanged(i, &ft)) r.dateRecentlyChanged = ft2dt(ft);
		
		// 属性
		DWORD attr = 0;
		if (pGetResultAttributes(i, &attr)) r.attributes = attr;
		
		// 运行次数
		if (pGetResultRunCount) r.runCount = pGetResultRunCount(i);
		
		// 扩展名
		wchar_t extBuf[64] = {};
		if (pGetResultExtensionW) pGetResultExtensionW(i, extBuf, 64);
		r.extension = QString::fromWCharArray(extBuf);
		
		// 类型
		r.isFolder = pIsFolderResult(i) != FALSE;
		
		results.append(std::move(r));
	}
	
	// ★ 统计（无条件更新 m_lastStats）
	m_lastStats.totalResults = pGetTotResults       ? pGetTotResults()       : n;
	m_lastStats.totalFiles   = pGetTotFileResults   ? pGetTotFileResults()   : 0;
	m_lastStats.totalFolders = pGetTotFolderResults ? pGetTotFolderResults() : 0;
	{
		quint64 totalSize = 0;
		for (const auto &r : results) totalSize += r.size;
		m_lastStats.totalSizeBytes = totalSize;
	}
	
	if (statsOut) *statsOut = m_lastStats;
	
	m_lastError.clear();
	return results;
}
