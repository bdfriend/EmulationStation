#include "Settings.h"
#include "Log.h"
#include "pugixml/src/pugixml.hpp"
#include "platform.h"
#include <boost/filesystem.hpp>
#include <boost/assign.hpp>

Settings* Settings::sInstance = NULL;

// these values are NOT saved to es_settings.xml
// since they're set through command-line arguments, and not the in-program settings menu
std::vector<const char*> settings_dont_save = boost::assign::list_of
	("Debug")
	("DebugGrid")
	("DebugText")
	("ShowExit")
	("Windowed")
	("VSync")
	("HideConsole")
	("IgnoreGamelist")
	("SplashScreen");

Settings::Settings()
{
	setDefaults();
	loadFile();
}

Settings* Settings::getInstance()
{
	if(sInstance == NULL)
		sInstance = new Settings();

	return sInstance;
}

void Settings::setDefaults()
{
	mBoolMap.clear();
	mIntMap.clear();

	mBoolMap["BackgroundJoystickInput"] = false;
	mBoolMap["ParseGamelistOnly"] = false;
	mBoolMap["ShowHiddenFiles"] = false;
	mBoolMap["DrawFramerate"] = false;
	mBoolMap["ShowExit"] = true;
	mBoolMap["Windowed"] = false;
	mBoolMap["SplashScreen"] = true;

#ifdef _RPI_
	// don't enable VSync by default on the Pi, since it already
	// has trouble trying to render things at 60fps in certain menus
	mBoolMap["VSync"] = false;
#else
	mBoolMap["VSync"] = true;
#endif

	mBoolMap["EnableSounds"] = true;
	mBoolMap["ShowHelpPrompts"] = true;
	mBoolMap["ScrapeRatings"] = true;
	mBoolMap["IgnoreGamelist"] = false;
	mBoolMap["HideConsole"] = true;
	mBoolMap["QuickSystemSelect"] = true;
	mBoolMap["MoveCarousel"] = true;
	mBoolMap["SaveGamelistsOnExit"] = true;

	mBoolMap["Debug"] = false;
	mBoolMap["DebugGrid"] = false;
	mBoolMap["DebugText"] = false;

	mIntMap["ScreenSaverTime"] = 5*60*1000; // 5 minutes
	mIntMap["ScraperResizeWidth"] = 400;
	mIntMap["ScraperResizeHeight"] = 0;
	#ifdef _RPI_
		mIntMap["MaxVRAM"] = 80;
	#else
		mIntMap["MaxVRAM"] = 100;
	#endif

	mStringMap["TransitionStyle"] = "fade";
	mStringMap["ThemeSet"] = "";
	mStringMap["ScreenSaverBehavior"] = "dim";
	mStringMap["Scraper"] = "TheGamesDB";
	mStringMap["GamelistViewStyle"] = "automatic";

	mBoolMap["ScreenSaverControls"] = true;
	mStringMap["ScreenSaverGameInfo"] = "never";
	mBoolMap["StretchVideoOnScreenSaver"] = false;
	mStringMap["PowerSaverMode"] = "disabled";

	// This setting only applies to raspberry pi but set it for all platforms so
	// we don't get a warning if we encounter it on a different platform
	mBoolMap["VideoOmxPlayer"] = false;
	#ifdef _RPI_
		// we're defaulting to OMX Player for full screen video on the Pi
		mBoolMap["ScreenSaverOmxPlayer"] = true;
	#else
		mBoolMap["ScreenSaverOmxPlayer"] = false;
	#endif

	mIntMap["ScreenSaverSwapVideoTimeout"] = 30000;

	mBoolMap["VideoAudio"] = true;
	mBoolMap["CaptionsCompatibility"] = true;
	// Audio out device for Video playback using OMX player.
	mStringMap["OMXAudioDev"] = "both";
	mStringMap["CollectionSystemsAuto"] = "";
	mStringMap["CollectionSystemsCustom"] = "";
	mBoolMap["SortAllSystems"] = false;
	mBoolMap["UseCustomCollectionsSystem"] = true;

	// Audio out device for volume control
	#ifdef _RPI_
		mStringMap["AudioDevice"] = "PCM";
	#else
		mStringMap["AudioDevice"] = "Master";
	#endif

}

template <typename K, typename V>
void saveMap(pugi::xml_document& doc, std::map<K, V>& map, const char* type)
{
	for(auto iter = map.begin(); iter != map.end(); iter++)
	{
		// key is on the "don't save" list, so don't save it
		if(std::find(settings_dont_save.begin(), settings_dont_save.end(), iter->first) != settings_dont_save.end())
			continue;

		pugi::xml_node node = doc.append_child(type);
		node.append_attribute("name").set_value(iter->first.c_str());
		node.append_attribute("value").set_value(iter->second);
	}
}

void Settings::saveFile()
{
	const std::string path = getHomePath() + "/.emulationstation/es_settings.cfg";

	pugi::xml_document doc;

	saveMap<std::string, bool>(doc, mBoolMap, "bool");
	saveMap<std::string, int>(doc, mIntMap, "int");
	saveMap<std::string, float>(doc, mFloatMap, "float");

	//saveMap<std::string, std::string>(doc, mStringMap, "string");
	for(auto iter = mStringMap.begin(); iter != mStringMap.end(); iter++)
	{
		pugi::xml_node node = doc.append_child("string");
		node.append_attribute("name").set_value(iter->first.c_str());
		node.append_attribute("value").set_value(iter->second.c_str());
	}

	doc.save_file(path.c_str());
}

void Settings::loadFile()
{
	const std::string path = getHomePath() + "/.emulationstation/es_settings.cfg";

	if(!boost::filesystem::exists(path))
		return;

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(path.c_str());
	if(!result)
	{
		LOG(LogError) << "Could not parse Settings file!\n   " << result.description();
		return;
	}

	for(pugi::xml_node node = doc.child("bool"); node; node = node.next_sibling("bool"))
		setBool(node.attribute("name").as_string(), node.attribute("value").as_bool());
	for(pugi::xml_node node = doc.child("int"); node; node = node.next_sibling("int"))
		setInt(node.attribute("name").as_string(), node.attribute("value").as_int());
	for(pugi::xml_node node = doc.child("float"); node; node = node.next_sibling("float"))
		setFloat(node.attribute("name").as_string(), node.attribute("value").as_float());
	for(pugi::xml_node node = doc.child("string"); node; node = node.next_sibling("string"))
		setString(node.attribute("name").as_string(), node.attribute("value").as_string());
}

//Print a warning message if the setting we're trying to get doesn't already exist in the map, then return the value in the map.
#define SETTINGS_GETSET(type, mapName, getMethodName, setMethodName) type Settings::getMethodName(const std::string& name) \
{ \
	if(mapName.find(name) == mapName.end()) \
	{ \
		LOG(LogError) << "Tried to use unset setting " << name << "!"; \
	} \
	return mapName[name]; \
} \
void Settings::setMethodName(const std::string& name, type value) \
{ \
	mapName[name] = value; \
}

SETTINGS_GETSET(bool, mBoolMap, getBool, setBool);
SETTINGS_GETSET(int, mIntMap, getInt, setInt);
SETTINGS_GETSET(float, mFloatMap, getFloat, setFloat);
SETTINGS_GETSET(const std::string&, mStringMap, getString, setString);
