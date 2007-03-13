#ifndef _BSETTINGS_H
#define _BSETTINGS_H

#ifndef Bool
#define Bool unsigned int
#endif

#define BSLIST_HDR(type,dtype)		\
typedef struct _BS##type##List	BS##type##List;\
struct _BS##type##List	\
{								\
	dtype   * data;			\
	BS##type##List * next;		\
}; \
BS##type##List * bs##type##ListAppend (BS##type##List * list, dtype *data); \
BS##type##List * bs##type##ListPrepend (BS##type##List * list, dtype *data);

typedef struct _BSContext			BSContext;
typedef struct _BSPlugin			BSPlugin;
typedef struct _BSSetting			BSSetting;
typedef struct _BSGroup				BSGroup;
typedef struct _BSSubGroup			BSSubGroup;
typedef struct _BSBackend			BSBackend;
typedef struct _BSBackendVTable 	BSBackendVTable;
typedef struct _BSPluginCategory	BSPluginCategory;
typedef struct _BSSettingValue		BSSettingValue;

BSLIST_HDR(Plugin,BSPlugin)
BSLIST_HDR(Setting,BSSetting)
BSLIST_HDR(String,char)
BSLIST_HDR(Group,BSGroup)
BSLIST_HDR(SubGroup,BSSubGroup)
BSLIST_HDR(SettingValue,BSSettingValue)


struct _BSContext
{
	BSPluginList *		plugins;
	BSPluginCategory *	categories;
	void *				privatePtr;

	BSBackend *			backend;

	char *				profile;
	Bool				deIntegration;

	BSSettingList * 	changedSettings;
	Bool 				pluginsChanged;
};

struct _BSBackend
{
	void *				dlHand;
	BSBackendVTable *	vtable;
};

struct _BSBackendVTable
{
	char *				name;
	char *				shortDesc;
	char *				longDesc;
	Bool				integrationSupport;
	Bool				profileSupport;
/*
	BSExecuteEventsFunc		executeEvents; // something like a event loop call for the backend
	// so it can check for file changes (gconf changes in the gconf backend)
	// no need for reload settings signals anymore

	BSInitBackendFunc	        backendInit;
	BSFiniBackendFunc		backendFini;

    	BSContextReadInitFunc		readInit;
    	BSContextReadSettingFunc	readSetting;
    	BSContextReadDoneFunc		readDone;

    	BSContextWriteInitFunc		writeInit;
    	BSContextWriteSettingFunc	writeSetting;
    	BSContextWriteDoneFunc		writeDone;


	BSGetIsIntegratedFunc 		getSettingIsIntegrated;
	BSGetIsReadOnlyFunc 		getSettingIsReadOnly;

	BSGetExistingProfilesFunc	getExistingProfiles;
	BSDeleteProfileFunc		deleteProfile;*/
};

struct _BSPlugin
{
	char *				name;
	char *				shortDesc;		// in current locale
	char *				longDesc;		// in current locale
	char *				hints;
	char *				category;		// simple name
	char *				filename;		// filename of the so
	
	BSStringList *		loadAfter;
	BSStringList *		loadBefore;
	BSStringList *		provides;
	BSStringList *		requires;
	BSSettingList *		settings;
	BSGroupList   *		groups;
	void *				privatePtr;
	BSContext *			context;
};

typedef enum _BSSettingType
{
 	TypeBool,
	TypeInt,
	TypeFloat,
	TypeString,
	TypeAction,
	TypeColor,
	TypeMatch,
	TypeList,
	TypeNum
} BSSettingType;

struct _BSSubGroup
{
	char *				name;		//in current locale
	char *				desc;		//in current locale
	BSSettingList *		settings;	//list of BerylSetting
};

struct _BSGroup
{
	char *				name;		//in current locale
	char *				desc;		//in current locale
	BSSubGroupList *	subGroups;	//list of BerylSettingsSubGroup
};

typedef enum _BSConflictType
{
	ConflictKey,
	ConflictButton,
	ConflictEdge,
	ConflctAny,
} BSConflictType;

typedef struct _BSSettingConflict
{
	BSSettingList *		settings;	// settings that conflict over the binding
	BSConflictType		type;		// type of the conflict, note that a setting may show up again in another
									// list for a different type
} BSConflict;

union _BSSettingInfo;

typedef struct _BSSettingIntInfo
{
	int		min;
	int		max;
} BSSettingIntInfo;

typedef struct _BSSettingFloatInfo
{
	float	min;
	float	max;
	float	precision;
} BSSettingFloatInfo;

typedef struct _BSSettingStringInfo
{
	BSStringList *	allowed_values; //list_of(char *) in current locale
} BSSettingStringInfo;

typedef struct _BSSettingActionInfo
{
	Bool	key;
	Bool	button;
	Bool	bell;
	Bool	edge;
} BSSettingActionInfo;

typedef struct _BSSettingActionArrayInfo
{
	Bool	array[4];
} BSSettingActionArrayInfo;

typedef struct _BSSettingListInfo
{
	BSSettingType			listType;
	union _BSSettingInfo *	listInfo;
} BSSettingListInfo;

typedef union _BSSettingInfo
{
	BSSettingIntInfo			forInt;
	BSSettingFloatInfo			forFloat;
	BSSettingStringInfo			forString;
	BSSettingActionInfo			forAction;
	BSSettingActionArrayInfo	forActionAsArray;
	BSSettingListInfo			forList;
} BSSettingInfo;

typedef struct _BSSettingColorValueColor
{
	unsigned short	red;
	unsigned short	green;
	unsigned short	blue;
	unsigned short	alpha;
} BSSettingColorValueColor;

typedef struct _BSSettingColorValueArray
{
	unsigned short	array[4];
} BSSettingColorValueArray;

typedef union _BSSettingColorValue
{
	BSSettingColorValueColor	color;
	BSSettingColorValueArray	array;
} BSSettingColorValue;


typedef struct _BSSettingActionValue
{
	int				button;
	unsigned int	buttonModMask;
	int				keysym;
	unsigned int	keyModMask;
	Bool			onBell;
	int				edgeMask;
} BSSettingActionValue;

typedef union _BSSettingValueUnion
{
	Bool					asBool;
	int						asInt;
	float					asFloat;
	char *					asString;
	char *					asMatch;
	BSSettingActionValue	asAction;
	BSSettingColorValue		asColor;
	BSSettingValueList *	asList;        // list_of(BerylSettingValue *)
} BSSettingValueUnion;

struct _BSSettingValue
{
	BSSettingValueUnion		value;
	BSSetting *				parent;
	Bool					isListChild;
};

struct _BSSetting
{
	char * 				name;
	char *				shortDesc;        // in current locale
	char *				longDesc;        // in current locale

	BSSettingType		type;
	Bool				isScreen;        // support the 'screen/display' thing
	unsigned int		screenNum;

	BSSettingInfo		info;
	char *				group;		// in current locale
	char *				subGroup;		// in current locale
	char * 				displayHints;	// in current locale

	BSSettingValue		default_value;
	BSSettingValue *	value; // = &default_value if isDefault == TRUE
	Bool				isDefault;

	BSPlugin *			parent;
	void * 				privatePtr;
};

struct _BSPluginCategory
{
	const char *		name;
	const char *		shortDesc;
	const char *		longDesc;
	BSStringList *		plugins;
};


BSContext * bsContextNew(void);


#endif
