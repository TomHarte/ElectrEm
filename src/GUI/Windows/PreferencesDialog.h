#pragma once

class PreferencesDialog
{
private:
	BasicConfigurationStore * _store;
	HWND _hPreferences;

public:
	PreferencesDialog(  HINSTANCE hInstance, HWND hMainWnd, BasicConfigurationStore * store );
	virtual ~PreferencesDialog(void);

	void Show();
	void Hide();
	void InitialisePreferenceDialog( HWND hDlg );

	void RefreshPreferences();
	void UpdatePreferences();
	void RefreshChecks();
protected:
	void Disable( UINT ctrlID, bool disable );
	bool GetCheck( UINT checkID );
	void SetCheck( UINT checkID, bool value );
	int GetSlider( UINT checkID );
	void SetSlider( UINT checkID, int value );

	void CreateFileType( LPCTSTR fileExt, LPCTSTR extGrp, LPCTSTR typeName, int iconNum );
	void DeleteFileType( LPCTSTR fileExt, LPCTSTR extGrp );
	bool FileTypeRegistered ( LPCTSTR fileExt, LPCTSTR extGrp );


};
