/*
 *  ConfigurationStore.h
 *  ElectrEm
 *
 *  Created by Ewen Roberts on Sat Jul 17 2004.
 *
 */
 
 #if !defined( _CONFIGURATIONSTORE_H )
 #define  _CONFIGURATIONSTORE_H


// A basic class from which other configurationstores derive. It provides an interface that 
// Reads and writes a set of values to the store.  The values are identified by name, and when
// read a default can be specified.

// This is used to reduce the complexity of code that reads a preference, allowing the code
// to handle a lack of the setting by using some sensible default.

class BasicConfigurationStore
{
public:	
	virtual ~BasicConfigurationStore() {};
	virtual bool ReadBool( const char * settingName, bool defaultValue ) = 0;
	virtual void WriteBool( const char * settingName, bool settingValue ) = 0;

	virtual int ReadInt( const char * settingName, int defaultValue ) = 0;
	virtual void WriteInt( const char * settingName, int settingValue ) = 0;
	
	virtual char * ReadString( const char * settingName, const char * defaultValue ) = 0;
	virtual void WriteString( const char * settingName, const char * settingValue ) = 0;

	virtual void Flush() = 0;
};

// A class that implements BasicConfigurationStore using Thomas Harte's CConfig class.

class CConfig;
class BasicHostMachine;
class ConfigurationStore : public BasicConfigurationStore
{
protected:
	CConfig * m_storage;
public:
	ConfigurationStore(const char * fileName );
	virtual ~ConfigurationStore();
	
	virtual bool ReadBool( const char * settingName, bool defaultValue );
	virtual void WriteBool( const char * settingName, bool settingValue );

	virtual int ReadInt( const char * settingName, int defaultValue );
	virtual void WriteInt( const char * settingName, int settingValue );
	
	virtual char * ReadString( const char * settingName, const char * defaultValue );
	virtual void WriteString( const char * settingName, const char * settingValue );

	virtual void Flush();
};

#endif
