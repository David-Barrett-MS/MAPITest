/*
 * By David Barrett, Microsoft Ltd. 2025. Use at your own risk.  No warranties are given.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * */

// MAPITest.cpp : This file contains the 'wmain' function. Program execution begins and ends there.
//

using namespace std;

#include <iostream>
#include <fstream>
#include <tchar.h>
#include <locale>
#include <codecvt>
#include <iostream>
#include <cwchar>
#include <conio.h>

#define USES_IID_IMAPIFolder
#define USES_IID_IMAPITable
#include <Mapix.h>
#include <mapiutil.h>
#define MDB_ONLINE				((ULONG) 0x00000100)

fstream logFile;
LPMAPISESSION lpSession = NULL;

LPWSTR lpszProfile = nullptr;
bool blnBackgroundSession = false;
bool blnNewSession = false;
bool blnAllowOthers = false;
bool blnListMessages = false;
bool blnListMessageProperties = false;
bool blnKeepSessionOpen = false;
int iPropDumpMessageCount = 0;


void log(string data)
{
	std::cout << data;
	if (logFile)
		logFile << data;
}

void log(LPWSTR lpszW)
{
	string data = wstring_convert<codecvt_utf8<wchar_t>, wchar_t>().to_bytes(wstring(lpszW));
	std::cout << data << "\n";
	if (logFile)
		logFile << data;
}

void log(string data, LPWSTR lpszW)
{
	string moreData = wstring_convert<codecvt_utf8<wchar_t>, wchar_t>().to_bytes(wstring(lpszW));
	std::cout << data << moreData << "\n";
	if (logFile)
		logFile << data << moreData << "\n";
}

void log(string data, ULONG number)
{
	std::cout << data << std::hex << number << "\n";
	if (logFile)
		logFile << data << std::hex << number << "\n";
}

void logError(string data, HRESULT hr)
{
	std::cout << data << hr << "\n";
	if (logFile)
		logFile << data << hr << "\n";
}

void logError(string data, HRESULT hr, LPWSTR lpszW)
{
	string moreData = wstring_convert<codecvt_utf8<wchar_t>, wchar_t>().to_bytes(wstring(lpszW));
	std::cout << data << moreData << "  hr=" << hr << "\n";
	if (logFile)
		logFile << data << moreData << "  hr=" << hr << "\n";
}

enum { EID, EMAIL_ADDRESS, DISPLAY_NAME, DEFAULT_STORE, NUM_COLS };

// Following adapted from https://learn.microsoft.com/en-us/outlook/troubleshoot/development/how-to-list-messages-in-inbox-with-mapi

/// <summary>
/// List the subject of each item found in the given folder
/// </summary>
/// <param name="lpMDB">Message store</param>
/// <param name="lpMessageFolder">Folder</param>
/// <returns>HRESULT</returns>
STDMETHODIMP ListMessages(
	LPMDB lpMDB,
	LPMAPIFOLDER lpMessageFolder)
{
	HRESULT hRes = S_OK;
	LPMAPITABLE lpContentsTable = NULL;
	LPSRowSet pRows = NULL;
	LPSTREAM lpStream = NULL;
	ULONG i;
	ULONG cMaxMessages = 100;

	// Define a SPropTagArray array here using the SizedSPropTagArray Macro
	// This enum allows access to portions of the array by name instead of number.
	// If more tags are added to the array, appropriate constants need to be added to the enum.
	enum {
		ePR_SUBJECT,
		ePR_ENTRYID,
		MSG_NUM_COLS
	};
	// These tags represent the message information we want to retrieve
	static SizedSPropTagArray(MSG_NUM_COLS, sptCols) = { MSG_NUM_COLS,
		PR_SUBJECT,
		PR_ENTRYID
	};

	log("Attempting to list items in folder\n");
	hRes = lpMessageFolder->GetContentsTable(
		0,
		&lpContentsTable);
	if (FAILED(hRes))
	{
		logError("Failed on GetContentsTable: ", hRes);
		goto quit;
	}

	hRes = HrQueryAllRows(
		lpContentsTable,				//
		(LPSPropTagArray)&sptCols,		//
		NULL,							// restriction
		NULL,							// sort order
		0,								//
		&pRows);						//
	if (FAILED(hRes))
	{
		logError("Failed on HrQueryAllRows: ", hRes);
		goto quit;
	}

	if (pRows->cRows < 100) { cMaxMessages = pRows->cRows; }
	for (i = 0; i < cMaxMessages; i++)
	{
		LPMESSAGE lpMessage = NULL;
		ULONG ulObjType = NULL;
		LPSPropValue lpProp = NULL;

		if (PR_SUBJECT == pRows->aRow[i].lpProps[ePR_SUBJECT].ulPropTag)
			log("Item subject: ", pRows->aRow[i].lpProps[ePR_SUBJECT].Value.lpszW);

		hRes = lpMDB->OpenEntry(
			pRows->aRow[i].lpProps[ePR_ENTRYID].Value.bin.cb,				// size of EntryId
			(LPENTRYID)pRows->aRow[i].lpProps[ePR_ENTRYID].Value.bin.lpb,	// value of EntryId
			NULL,															// default interface (which will return IMessage in this case)
			MAPI_BEST_ACCESS,												// flags
			&ulObjType,														// pointer to the type of opened object [OUT]
			(LPUNKNOWN*)&lpMessage);										// pointer to a pointer to the opened object [OUT]

		if (!FAILED(hRes))
		{
			// We've opened the message
			if (blnListMessageProperties)
			{
				if (iPropDumpMessageCount < 5)
				{
					// Retrieve and log all properties
					ULONG ulProps = 0;
					LPSPropValue propArray = nullptr;
					hRes = lpMessage->GetProps(NULL, MAPI_UNICODE, &ulProps, &propArray);
					if (SUCCEEDED(hRes))
					{
						// Process the properties
						for (ULONG i = 0; i < ulProps; ++i)
						{
							log("Property: ", propArray[i].ulPropTag);
						}

						// Free the property array
						MAPIFreeBuffer(propArray);
						iPropDumpMessageCount++;
					}
					else
					{
						logError("Failed to retrieve message properties: ", hRes);
					}
				}
			}
		}
		else
			logError("OpenEntry error: ", hRes);

		MAPIFreeBuffer(lpProp);
		UlRelease(lpMessage);
		hRes = S_OK;
	}

quit:
	FreeProws(pRows);
	UlRelease(lpContentsTable);
	return hRes;
}

/// <summary>
/// Attempt to open the receive folder for the given message store
/// </summary>
/// <param name="lpMDB">The message store</param>
/// <param name="lpInboxFolder">Receive folder (if found)</param>
/// <returns>HRESULT</returns>
STDMETHODIMP OpenInbox(
	LPMDB lpMDB,
	LPMAPIFOLDER* lpInboxFolder)
{
	ULONG cbInbox;
	LPENTRYID lpbInbox;
	ULONG ulObjType;
	HRESULT hRes = S_OK;
	LPMAPIFOLDER lpTempFolder = NULL;
	LPSPropValue tmp = NULL;

	*lpInboxFolder = NULL;

	hRes = lpMDB->GetReceiveFolder(
		NULL,			// Get default receive folder
		NULL,			// Flags
		&cbInbox,		// Size of receive folder
		&lpbInbox,		// EntryId of receive folder
		NULL);			//
	if (FAILED(hRes))
	{
		logError("Failed on GetReceiveFolder: ", hRes);
		goto quit;
	}

	hRes = lpMDB->OpenEntry(
		cbInbox,					// Size and...
		lpbInbox,					// Value of the Inbox's EntryID
		NULL,						// We want the default interface (IMAPIFolder)
		MAPI_BEST_ACCESS,			// Flags
		&ulObjType,					// Object returned type
		(LPUNKNOWN*)&lpTempFolder); // Returned folder
	if (FAILED(hRes))
	{
		logError("Failed on OpenEntry (receive folder): ", hRes);
		goto quit;
	}

	// Retrieve and log the name of the folder we have opened
	hRes = HrGetOneProp(
		lpTempFolder,
		PR_DISPLAY_NAME,
		&tmp);
	if (FAILED(hRes)) goto quit;
	log("Opened receive folder: ", tmp->Value.lpszW);

	// Assign the out parameter
	*lpInboxFolder = lpTempFolder;

quit:
	if (tmp) MAPIFreeBuffer(tmp);
	MAPIFreeBuffer(lpbInbox);
	return hRes;
}

/// <summary>
/// Process the specified message store (attempts to open store and then list messages in default receive folder)
/// </summary>
/// <param name="storeInfoRow">SRow from stores table containing message store info (e.g. EntryId)</param>
/// <returns>HRESULT</returns>
HRESULT ProcessMessageStore(SRow storeInfoRow)
{
	LPMDB       pMDB = NULL;

	HRESULT hr = lpSession->OpenMsgStore(NULL,
		storeInfoRow.lpProps[EID].Value.bin.cb,
		(LPENTRYID)storeInfoRow.lpProps[EID].Value.bin.lpb,
		NULL,
		MAPI_BEST_ACCESS | MDB_NO_DIALOG | MDB_ONLINE,
		&pMDB);

	if (SUCCEEDED(hr))
	{
		// We have a message store, so try to open the root folder
		log("Opened message store: ", storeInfoRow.lpProps[DISPLAY_NAME].Value.lpszW);

		LPUNKNOWN lpUnk = NULL;
		ULONG ulType = 0;
		hr = pMDB->OpenEntry(0, NULL, NULL, MAPI_BEST_ACCESS, &ulType, &lpUnk);
		if (SUCCEEDED(hr))
		{
			LPMAPIFOLDER msgStoreRoot = (LPMAPIFOLDER)lpUnk;
			log("Opened root folder of message store: ", storeInfoRow.lpProps[DISPLAY_NAME].Value.lpszW);

			// Open inbox
			LPMAPIFOLDER inbox;
			hr = OpenInbox(pMDB, &inbox);
			if (SUCCEEDED(hr))
			{
				log("Receive folder opened\n");
				if (blnListMessages)
					ListMessages(pMDB, inbox);
				// Release inbox
				inbox->Release();
				log("Receive folder released\n");
			}

			msgStoreRoot->Release();
			log("Released root folder of message store: ", storeInfoRow.lpProps[DISPLAY_NAME].Value.lpszW);
		}
		else
			logError("Failed to open root folder of message store: ", hr, storeInfoRow.lpProps[DISPLAY_NAME].Value.lpszW);

		pMDB->Release();
		log("Released message store: ", storeInfoRow.lpProps[DISPLAY_NAME].Value.lpszW);
	}
	else
		logError("Failed on OpenMsgStore: ", hr, storeInfoRow.lpProps[DISPLAY_NAME].Value.lpszW);

	return hr;
}

/// <summary>
/// Initialise MAPI, log on and read the message store table
/// </summary>
/// <returns>0 if successful, error code otherwise</returns>
int MAPITest()
{
	int nRetCode = 0;
	HRESULT hRes = 0;
	//LPWSTR lpszProfile = NULL;

	log("Initialising MAPI test\n");

	HMODULE hModule = ::GetModuleHandle(NULL);

	if (hModule != NULL)
	{
		// Create log file
		log("Creating log file\n");
		logFile.open("MAPITest Log.txt", ios::out);
		if (!logFile)
			log("Failed to create log file\n");

		log("Starting MAPI log-on test\n");

		const int MAPI_BG_SESSION = 0x00200000;

		// Initialize MAPI
		if FAILED(hRes = MAPIInitialize(0))
		{
			logError("Fatal Error: MAPIInitialize failed ", hRes);
			nRetCode = hRes;
		}
		else
		{
			log("MAPIInitialize succeeded\n");
			FLAGS flags = MAPI_EXTENDED;
			if (blnBackgroundSession)
			{
				flags = flags | MAPI_BG_SESSION;
				log("Using MAPI_BG_SESSION\n");
			}
			if (blnNewSession)
			{
				flags = flags | MAPI_NEW_SESSION;
				log("Using MAPI_NEW_SESSION\n");
			}
			if (blnAllowOthers)
			{
				flags = flags | MAPI_ALLOW_OTHERS;
				log("Using MAPI_ALLOW_OTHERS\n");
			}
			if (lpszProfile == nullptr)
			{
				flags = flags | MAPI_USE_DEFAULT;
				log("Using MAPI_USE_DEFAULT\n");
			}
			else
			{
				log("Attempting to use MAPI profile: ");
				log(lpszProfile);
			}

			// Log on to MAPI session
			if (FAILED(hRes = MAPILogonEx(NULL, lpszProfile, NULL, flags, &lpSession)))
				logError("Error: MAPILogonEx failed ", hRes);
			else
			{
				log("MAPILogonEx succeeded\n");

				// Retrieve list of message stores
				LPMAPITABLE pStoresTbl = NULL;
				SizedSPropTagArray(NUM_COLS, sptCols) = { NUM_COLS, PR_ENTRYID, PR_EMAIL_ADDRESS, PR_DISPLAY_NAME, PR_DEFAULT_STORE };

				HRESULT hr = lpSession->GetMsgStoresTable(0, &pStoresTbl);
				if (FAILED(hr))
				{
					nRetCode = hr;
					logError("GetMsgStoresTable failed: ", hr);
				}
				else
				{
					log("GetMsgStoresTable succeeded\n");

					LPSRowSet   rows = NULL;
					LPMDB       pMDB = NULL;
					hr = HrQueryAllRows(pStoresTbl, (LPSPropTagArray)&sptCols, NULL, NULL, 0, &rows);
					if (SUCCEEDED(hr))
					{
						// We have a list of message stores... Display them, and attempt to open the default store
						for (unsigned int i = 0; i < rows->cRows; i++)
						{
							if (rows->aRow[i].lpProps[DEFAULT_STORE].Value.b)
							{
								// This is the default store
								log("Default message store: ", rows->aRow[i].lpProps[DISPLAY_NAME].Value.lpszW);
								ProcessMessageStore(rows->aRow[i]);
							}
							else
								log("Message store: ", rows->aRow[i].lpProps[DISPLAY_NAME].Value.lpszW);
						}
					}
					if (rows) FreeProws(rows);
					pStoresTbl->Release();
				}

				if (blnKeepSessionOpen)
				{
					log("Waiting for keypress before logging session off");
					_getwch();
					log("\n");
				}

				// Log off
				if (FAILED(hRes = lpSession->Logoff(0, 0, 0)))
					logError("Error at Logoff: ", hRes);
				else
					log("LogOff succeeded\n");
			}
			if (lpSession) lpSession->Release();
			MAPIUninitialize();
		}
		logFile.close();
	}
	return nRetCode;
}

/// <summary>
/// Main entry point
/// </summary>
int wmain(int argc, wchar_t* argv[])
{
	// Parse any command line arguments
	for (int i = 0; i < argc; ++i)
	{
		if (std::wcscmp(argv[i], L"-profile") == 0 && i + 1 < argc)
		{
			lpszProfile = argv[i + 1];
		}
		else if (std::wcscmp(argv[i], L"-bgsession") == 0)
		{
			blnBackgroundSession = true;
		}
		else if (std::wcscmp(argv[i], L"-newsession") == 0)
		{
			blnNewSession = true;
		}
		else if (std::wcscmp(argv[i], L"-allowothers") == 0)
		{
			blnAllowOthers = true;
		}
		else if (std::wcscmp(argv[i], L"-listmessages") == 0)
		{
			blnListMessages = true;
		}
		else if (std::wcscmp(argv[i], L"-listmessageproperties") == 0)
		{
			blnListMessages = true;
			blnListMessageProperties = true;
		}
		else if (std::wcscmp(argv[i], L"-keepsessionopen") == 0)
		{
			blnKeepSessionOpen = true;
		}
	}

	return MAPITest();
}


