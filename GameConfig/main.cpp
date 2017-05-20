#include <system.h>
#include <dxerror.h>
#include <ckey.h>
#include <cconfigfile.h>
#include <cdisplay.h>
#include <ckeyboardlistener.h>
#include <cmsgreporter.h>
#include "resource.h"

/*
TODO:
-Enumérer les port audio et pouvoir changer le port utilisé lors de la lecture des sons et music
*/

#ifdef _DEBUG
#define WND_WIDTH 1024
#define WND_HEIGHT 768
#else
#define WND_WIDTH 800
#define WND_HEIGHT 600
#endif

struct KeyPanel
{
	HWND hKey, hKeyPanel, hModifiy, hDelete;
};

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ConfigKeyDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
HRESULT CALLBACK EnumDisplayMode(LPDDSURFACEDESC pDDSD, LPVOID Context);
DisplayMode ChangeGraphicConfig(HWND hDM, HWND hWinMode);
void ResetDisplayModeList(HWND hwnd);
void SelectFirstDisplayMode(HWND hwnd, Str ModeString);
KeyPanel CreateKeyPanel(int index, HWND hwnd);
void DestroyKeyPanel(KeyPanel *pKeyPanel);
void ResetScrollBar(HWND hwnd, int height);

HINSTANCE hinst;
Display Screen;
Input Keyboard;
ConfigFile Config;

int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	HWND hwnd;
	MSG msg;
	WNDCLASS wc;

	InitDXErrorMsg();

	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hinstance;
	wc.hIcon = LoadIcon(hinstance, MAKEINTRESOURCE(IDI_ICON1));
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(1 + COLOR_BTNFACE);
	wc.lpszMenuName =  MAKEINTRESOURCE(IDR_MENU1);
	wc.lpszClassName = "MainWinClass";
	if(!RegisterClass(&wc)) return 0;

	hwnd = CreateWindow("MainWinClass", "GameConfig", WS_OVERLAPPEDWINDOW | WS_VSCROLL, CW_USEDEFAULT, CW_USEDEFAULT, WND_WIDTH, WND_HEIGHT, NULL, NULL, hinstance, NULL);
	
	if (!hwnd)
		return 0;

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	Keyboard = Input(hwnd, hinstance);
	Keyboard.CreateDevice(GUID_SysKeyboard, &c_dfDIKeyboard, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 1;
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int i, j, id[3], ScrollPos1, ScrollPos2, ScrollMinPos, ScrollMaxPos;
	static int CurrentId;
	static bool CaptureKey;
	char buffer[1024];
	RECT ClientRect;
	static HWND hDisplayModes, hWinMode, hResFilters;
	Str Text;
	static KeyPanel KeyPanels[KeyboardListener::MAX_KEY];
	static DisplayMode DMData;
	KeyboardListener keybListener;

	switch (uMsg)
	{
		case WM_CREATE:
			CaptureKey = false;

			// On créé les fenêtres clientes
			CreateWindow("STATIC", "GRAPHIQUE", WS_CHILD | WS_VISIBLE, 10, 10, 200, 20, hwnd, NULL, hinst, NULL);
			CreateWindow("STATIC", "CLAVIER", WS_CHILD | WS_VISIBLE, 300, 10, 200, 20, hwnd, NULL, hinst, NULL);
			CreateWindow("STATIC", "SON", WS_CHILD | WS_VISIBLE, 10, 200, 200, 20, hwnd, NULL, hinst, NULL);
			hDisplayModes = CreateWindow("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 10, 60, 200, 400, hwnd, (HMENU)ID_GRAPHIC_RESOLUTION, hinst, NULL);
			hWinMode = CreateWindow("BUTTON", "Plein écran", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 10, 100, 100, 20, hwnd, (HMENU)ID_GRAPHIC_WINDOWED, hinst, NULL);
#ifdef _DEBUG
			CreateWindow("STATIC", "Filtres:", WS_CHILD | WS_VISIBLE, 10, 140, 200, 20, hwnd, NULL, hinst, NULL);
			hResFilters = CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 10, 160, 200, 20, hwnd, (HMENU)ID_GRAPHIC_FILTERS, hinst, NULL);
#endif
			CreateWindow("BUTTON", "Jouer la musique", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 10, 250, 200, 20, hwnd, (HMENU)ID_SOUND_MUSICSWITCH, hinst, NULL);
			CreateWindow("BUTTON", "Jouer les sons", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 10, 270, 200, 20, hwnd, (HMENU)ID_SOUND_SOUNDSWITCH, hinst, NULL);

			// On charge la config
			if (Config.Open(true, false, false))
			{
				Config.ReadGraphic(&DMData);
				Config.ReadModeFilters();
				Config.ReadKeys();
				if (Config.PlayMusic())
					CheckDlgButton(hwnd, ID_SOUND_MUSICSWITCH, BST_CHECKED);
				if (Config.PlaySounds())
					CheckDlgButton(hwnd, ID_SOUND_SOUNDSWITCH, BST_CHECKED);
				Config.Close();
			}

			// On énumére les modes d'affichage
			Screen.EnumDisplayMode(0, NULL, hDisplayModes, EnumDisplayMode);
			
			// On sélectionne les valeurs chargée puis on change la config graphique
			SelectFirstDisplayMode(hDisplayModes, *DMData.getName());
			SendMessage(hWinMode, BM_SETCHECK, !DMData.GetWindowed(), 0);

			// On créé un bouton pour chaque touche du clavier
			for (i = 0; i < Config.GetKeyMap()->GetElementCount(); i++)
				KeyPanels[i] = CreateKeyPanel(i, hwnd);

			// On met à jour le contenu dans le champs des filtres
			SetDlgItemText(hwnd, ID_GRAPHIC_FILTERS, Config.GetModeFilters());

#ifndef _DEBUG
			EnableMenuItem(GetMenu(hwnd), ID_MENU_ADDKEY, MF_GRAYED);
#endif
			return 0;
		case WM_SIZE:
			ResetScrollBar(hwnd, HIWORD(lParam));
			return 0;
		case WM_VSCROLL:
			switch (LOWORD(wParam))
			{
			case SB_THUMBPOSITION:
				// Lorsque on a lâché l'ascenseur
				ScrollPos1 = GetScrollPos(hwnd, SB_VERT);
				ScrollPos2 = HIWORD(wParam);
				SetScrollPos(hwnd, SB_VERT, ScrollPos2, FALSE);
				ScrollWindow(hwnd, 0, ScrollPos1-ScrollPos2, NULL, NULL);
				break;
			case SB_THUMBTRACK:
				// Lorsque on appuye sur l'ascenseur
				break;
			case SB_LINEUP:
				// Lorsque on appuye sur la flèche du haut
				if (GetScrollPos(hwnd, SB_VERT) > 0)
				{
					ScrollWindow(hwnd, 0, 5, NULL, NULL);
					SetScrollPos(hwnd, SB_VERT, GetScrollPos(hwnd, SB_VERT)-5, TRUE);
				}
				break;
			case SB_LINEDOWN:
				// Lorsque on appuye sur la flèche du bas
				GetScrollRange(hwnd, SB_VERT, &ScrollMinPos, &ScrollMaxPos);
				if (GetScrollPos(hwnd, SB_VERT) < ScrollMaxPos)
				{
					ScrollWindow(hwnd, 0, -5, NULL, NULL);
					SetScrollPos(hwnd, SB_VERT, GetScrollPos(hwnd, SB_VERT)+5, TRUE);
				}
				break;
			case SB_PAGEUP:
				// Lorsque on appuye sur la barre au-dessus de l'asceuseur
				ScrollWindow(hwnd, 0, 15, NULL, NULL);
				SetScrollPos(hwnd, SB_VERT, GetScrollPos(hwnd, SB_VERT)-15, TRUE);
				break;
			case SB_PAGEDOWN:
				// Lorsque on appuye sur la barre en-dessous de l'asceuseur
				ScrollWindow(hwnd, 0, -15, NULL, NULL);
				SetScrollPos(hwnd, SB_VERT, GetScrollPos(hwnd, SB_VERT)+15, TRUE);
				break;
			}
			return 0;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
			case ID_GRAPHIC_FILTERS:
				switch (HIWORD(wParam))
				{
				case EN_CHANGE:
					GetDlgItemText(hwnd, ID_GRAPHIC_FILTERS, buffer, 1024);
					Config.SetModeFilters(Str(buffer));
					ResetDisplayModeList(hDisplayModes);
					Screen.EnumDisplayMode(0, NULL, hDisplayModes, EnumDisplayMode);
					SelectFirstDisplayMode(hDisplayModes, *DMData.getName());
					break;
				}
				return 0;
			case ID_GRAPHIC_RESOLUTION:
				switch (HIWORD(wParam))
				{
				case CBN_SELCHANGE:
					DMData = ChangeGraphicConfig(hDisplayModes, hWinMode);
					return 0;
				default:
					return 0;
				}
				return 0;
			case ID_GRAPHIC_WINDOWED:
				DMData = ChangeGraphicConfig(hDisplayModes, hWinMode);
				return 0;
			case ID_MENU_DEFAULT:
				for (i = 0; i < Config.GetKeyMap()->GetElementCount(); i++)
				{
					Key::getKeyElement(i, Config.GetKeyMap())->setDefaultDik();
					SetDlgItemText(hwnd, ID_KEYB_KEYLABEL+i, Key::getKeyElement(i, Config.GetKeyMap())->label().Get());
				}
				return 0;
			case ID_MENU_ADDKEY:
				if (Config.GetKeyMap()->GetElementCount() < KeyboardListener::MAX_KEY)
				{
					Config.GetKeyMap()->AddElement(new Key());
					i = Config.GetKeyMap()->GetElementCount() - 1;
					if (DialogBoxParam(hinst, MAKEINTRESOURCE(IDD_CONFIGKEY), hwnd, (DLGPROC)ConfigKeyDlgProc, i))
					{
						GetClientRect(hwnd, &ClientRect);
						ResetScrollBar(hwnd, ClientRect.bottom);
						KeyPanels[i] = CreateKeyPanel(i, hwnd);
						if (GetScrollRange(hwnd, SB_VERT, &ScrollMinPos, &ScrollMaxPos))
						{
							ScrollPos1 = GetScrollPos(hwnd, SB_VERT);
							SetScrollPos(hwnd, SB_VERT, ScrollMaxPos, TRUE);
							ScrollWindow(hwnd, 0, ScrollPos1-ScrollMaxPos, NULL, NULL);
						}
					}
					else
						Config.GetKeyMap()->RemoveElement(i, true);
				}
				else
					MessageBox(hwnd, BuildString("Le nombre de touches ne peut dépasser %d", KeyboardListener::MAX_KEY).Get(), "Nombre de touches maximal atteint", MB_ICONERROR);
				return 0;
			case ID_MENU_SAVE:
				if (Config.Open(false, true, true))
				{
					Config.WriteGraphic(&DMData);
					Config.WriteModeFilters();
					Config.WriteKeys();
					Config.WriteSound(IsDlgButtonChecked(hwnd, ID_SOUND_MUSICSWITCH), IsDlgButtonChecked(hwnd, ID_SOUND_SOUNDSWITCH));
					Config.Close();
				}
				return 0;
			case ID_MENU_EXIT:
				SendMessage(hwnd, WM_DESTROY, NULL, NULL);
				return 0;
			default:
				// On cherche quel bouton est pressé
				for (i = 0; i < Config.GetKeyMap()->GetElementCount(); i++)
				{
					id[0] = ID_KEYB_KEY + i;
					id[1] = ID_KEYB_MODIFY + i;
					id[2] = ID_KEYB_DELETE + i;

					// Le bouton pour capturer une touche est pressé
					if (LOWORD(wParam) == id[0])
					{
						CurrentId = id[0];
						CaptureKey = true;
						SetDlgItemText(hwnd, id[0], "PRESSEZ UNE TOUCHE");
						SetFocus(hwnd);
						for (j = 0; j < Config.GetKeyMap()->GetElementCount(); j++)
							if (i != j)
								SetDlgItemText(hwnd, ID_KEYB_KEY+j, Key::getKeyElement(j, Config.GetKeyMap())->getName()->Get());
						return 0;
					}

					// Le bouton pour modifier une touche est pressé
					if (LOWORD(wParam) == id[1])
					{
						if (DialogBoxParam(hinst, MAKEINTRESOURCE(IDD_CONFIGKEY), hwnd, (DLGPROC)ConfigKeyDlgProc, i))
							SetDlgItemText(hwnd, id[0], Key::getKeyElement(i, Config.GetKeyMap())->getName()->Get());
						return 0;
					}

					// Le bouton pour supprimer une touche est pressé
					if (LOWORD(wParam) == id[2] && MessageBox(hwnd, "Supprimer la touche?", "Confirmation", MB_YESNO) == IDYES)
					{
						Config.GetKeyMap()->RemoveElement(i, 1, true);
						GetClientRect(hwnd, &ClientRect);
						ResetScrollBar(hwnd, ClientRect.bottom);
						// On supprime tous les boutons
						for (j = 0; j < KeyboardListener::MAX_KEY; j++)
							DestroyKeyPanel(&KeyPanels[j]);
						// On recréé ce qui reste
						for (j = 0; j < Config.GetKeyMap()->GetElementCount(); j++)
							KeyPanels[j] = CreateKeyPanel(j, hwnd);
						return 0;
					}
				}
				return 0;
			}
			return 0;
		case WM_KEYDOWN:
			if (CaptureKey)
			{
				i = CurrentId - ID_KEYB_KEY;
				CaptureKey = false;
				if (keybListener.startCaptureKeys(&Keyboard))
				{
					keybListener.captureKeys();
					Key::getKeyElement(i, Config.GetKeyMap())->setDik(keybListener.keyPressed());
					SetDlgItemText(hwnd, CurrentId, Key::getKeyElement(i, Config.GetKeyMap())->getName()->Get());
					SetDlgItemText(hwnd, ID_KEYB_KEYLABEL+i, Key::getKeyElement(i, Config.GetKeyMap())->label().Get());
				}
			}
			return 0;
		case WM_DESTROY:
			i = 0;
			if (MessageBox(hwnd, "Voulez-vous garder les changements?", "Fermeture", MB_YESNO) == IDYES)
				if (Config.Open(false, true, true))
				{
					Config.WriteGraphic(&DMData);
					Config.WriteModeFilters();
					Config.WriteKeys();
					Config.WriteSound(IsDlgButtonChecked(hwnd, ID_SOUND_MUSICSWITCH), IsDlgButtonChecked(hwnd, ID_SOUND_SOUNDSWITCH));
					Config.Close();
					i = 1;
				}
			Screen.Destroy();
			Keyboard.Destroy();
			SendMessage(HWND_BROADCAST, RegisterWindowMessage("GabGamesConfig"), i, 0);
			PostQuitMessage(0);
			return 0;
		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

LRESULT CALLBACK ConfigKeyDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static int index;
	char buffer[24];

	switch (msg)
	{
	case WM_INITDIALOG:
		index = (int)lParam;
		SetDlgItemText(hDlg, IDC_KEYLABEL, Key::getKeyElement(index, Config.GetKeyMap())->getName()->Get());
		return 0;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			GetDlgItemText(hDlg, IDC_KEYLABEL, buffer, 24);
			Key::getKeyElement(index, Config.GetKeyMap())->setName(buffer);
			EndDialog(hDlg, 1);
			return 0;
		case IDCANCEL:
			EndDialog(hDlg, 0);
			return 0;
		default:
			return 0;
		}
	default:
		return 0;
	}
}

HRESULT CALLBACK EnumDisplayMode(LPDDSURFACEDESC pDDSD, LPVOID Context)
{
	int data;
	HWND hDisplayModes = (HWND)Context;
	LRESULT lr;
	DisplayMode *NewDM;

	if (!Config.IsModeValid(pDDSD->dwWidth, pDDSD->dwHeight))
		return DDENUMRET_OK;

	data = Screen.GetElementCount();
	NewDM = new DisplayMode(pDDSD->dwWidth, pDDSD->dwHeight, pDDSD->ddpfPixelFormat.dwRGBBitCount);
	Screen.AddElement(NewDM);

	lr = SendMessage(hDisplayModes, CB_ADDSTRING, 0, (LPARAM)NewDM->getName()->Get());
	lr = SendMessage(hDisplayModes, CB_SETITEMDATA, data, (LPARAM)data);

	return DDENUMRET_OK;
}

DisplayMode ChangeGraphicConfig(HWND hDM, HWND hWinMode)
{
	int SelIndex, DataIndex;
	DisplayMode Config;

	// Résolution
	SelIndex = (int)SendMessage(hDM, CB_GETCURSEL, 0, 0); 
	DataIndex = (int)SendMessage(hDM, CB_GETITEMDATA, SelIndex, 0);
	Config.SetWidth(DisplayMode::getDisplayModeElement(DataIndex, &Screen)->GetWidth());
	Config.SetHeight(DisplayMode::getDisplayModeElement(DataIndex, &Screen)->GetHeight());
	Config.SetBit(DisplayMode::getDisplayModeElement(DataIndex, &Screen)->GetBit());

	// Plein écran
	switch (SendMessage(hWinMode, BM_GETCHECK, 0, 0))
	{
	case BST_CHECKED:
		Config.SetWindowed(false);
		break;
	case BST_UNCHECKED:
		Config.SetWindowed(true);
		break;
	default:
		Config.SetWindowed(false);
		break;
	}

	return Config;
}

void ResetDisplayModeList(HWND hwnd)
{
	Screen.RemoveAllElement(0, true);
	SendMessage(hwnd, CB_RESETCONTENT, 0, 0);
}

void SelectFirstDisplayMode(HWND hwnd, Str ModeString)
{
	int index;

	index = (int)SendMessage(hwnd, CB_FINDSTRINGEXACT, -1, (LPARAM)ModeString.Get());
	if (index == CB_ERR) index = 1;
	SendMessage(hwnd, CB_SETCURSEL, index, 0);
}

KeyPanel CreateKeyPanel(int index, HWND hwnd)
{
	KeyPanel Panel;

	Panel.hKey = CreateWindow("BUTTON", Key::getKeyElement(index, Config.GetKeyMap())->getName()->Get(), WS_CHILD | WS_VISIBLE, 300, 60+(index*20), 200, 20, hwnd, (HMENU)(ID_KEYB_KEY+index), hinst, NULL);
	Panel.hKeyPanel = CreateWindow("STATIC", Key::getKeyElement(index, Config.GetKeyMap())->label(), WS_CHILD | WS_VISIBLE, 520, 60+(index*20), 200, 20, hwnd, (HMENU)(ID_KEYB_KEYLABEL+index), hinst, NULL);
#ifdef _DEBUG
	Panel.hModifiy = CreateWindow("BUTTON", "Modifier", WS_CHILD | WS_VISIBLE, 740, 60+(index*20), 70, 20, hwnd, (HMENU)(ID_KEYB_MODIFY+index), hinst, NULL);
	Panel.hDelete = CreateWindow("BUTTON", "Supprimer", WS_CHILD | WS_VISIBLE, 830, 60+(index*20), 70, 20, hwnd, (HMENU)(ID_KEYB_DELETE+index), hinst, NULL);
#endif

	return Panel;
}

void DestroyKeyPanel(KeyPanel *pKeyPanel)
{
	DestroyWindow(pKeyPanel->hKey);
	DestroyWindow(pKeyPanel->hKeyPanel);
#ifdef _DEBUG
	DestroyWindow(pKeyPanel->hModifiy);
	DestroyWindow(pKeyPanel->hDelete);
#endif
}

void ResetScrollBar(HWND hwnd, int height)
{
	int pos, MaxPos;

	// On met l'ascenseur tout en haut pour éviter tout problème...
	pos = GetScrollPos(hwnd, SB_VERT);
	SetScrollPos(hwnd, SB_VERT, 0, TRUE);
	ScrollWindow(hwnd, 0, pos, NULL, NULL);

	// On définit les valeurs minimales et maximales du scrolling
	if (Config.GetKeyMap()->GetElementCount())
		MaxPos = 60 + Config.GetKeyMap()->GetElementCount() * 20 - height;
	else
		MaxPos = 0;
	SetScrollRange(hwnd, SB_VERT, 0, MaxPos, FALSE);
}
