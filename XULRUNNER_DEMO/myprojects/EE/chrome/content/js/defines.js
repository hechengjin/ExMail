const MC_MSG_INFO = 1;				// display information message
const MC_MSG_ALERT = 2;					// display alert message
const MC_MSG_CMD = 3;						// display command line
const MC_MSG_POPUP = 4;					// pop up a message
const MC_MSG_YESNOCANCEL = 5;				// display a YES/NO/CANCEL confirmation message
const MC_MSG_YESNO = 6;					// display a YES/NO confirmation message
const MC_UPDATE_LOG = 7;					// update log text display
const MC_UPDATE_PROGRESS = 8;				// update progress display
const MC_UPDATE_STATS = 9;				// update statistics display
const MC_UPDATE_TIME = 10;					// update time display
const MC_UPDATE_TITLE = 11;				// update window title
const MC_UPDATE_STATE = 12;				// update state display
const MC_UPDATE_UI = 13;					// update user interface
const MC_UPDATE_LIST = 14;					// update file list
const MC_WRITE_CONSOLE = 15;				// update console display
const MC_JOB_START = 16;					// indicate a job is to be started
const MC_JOB_FINISH = 17;					// indicate a job is to be finished
const MC_UI_CMD = 18;						// issue UI command
const MC_DEBUG = 19;

const INIT_OK = 0;
const INIT_ERROR = -1;
const INIT_INITED = 1;

const MODE_DVD = 1;
const MODE_VCD = 2;
const MODE_SVCD = 3;
const MODE_CD = 4;
const MODE_UNKNOWN = 5;

const PF_NOFILTER = 0x1000000;
const PF_PREVIEW = 0x2000000;
const PF_READ_OUTPUT = 0x4000000;
const PF_LEAVE_WINDOW_OPEN = 0x8000000;
const PF_NO_RESULT = 0x10000000;

const FLAG_DETACH = 1;

const INDEX_VIDEO = 0;
const INDEX_PAGES = 1;
const INDEX_LYRIC = 2;
const INDEX_SWF = 3;
const INDEX_TVU = 4;

// Medialib Categories
const CATEGORY_VIDEO = 0;
const CATEGORY_MUSIC = 1;
const CATEGORY_IMAGE = 2;
const CATEGORY_DEVICE = 3;
const CATEGORY_DOWNLOAD = 4;
const CATEGORY_NETTV = 5;

const VIEW_FULL = 1;
const VIEW_DEVICE = 2;
const VIEW_COMPACT = 3;
const VIEW_SIMPLE = 4;
const VIEW_TV = 5;
const VIEW_CENTER = 6;

const TV_START = 0;
const TV_FULL = 1;

const PLAY_TYPE_IDLE = 0;
const PLAY_TYPE_FILE = 1;
const PLAY_TYPE_URL = 2;
const PLAY_TYPE_TRACK = 3;
const PLAY_TYPE_SWF = 4;
const PLAY_TYPE_IMAGE = 5;
const PLAY_TYPE_TVU = 6;

// device related consts
const DBT_DEVICEARRIVAL               = 0x8000;  // system detected a new device
const DBT_DEVICEQUERYREMOVE           = 0x8001;  // wants to remove, may fail
const DBT_DEVICEQUERYREMOVEFAILED     = 0x8002;  // removal aborted
const DBT_DEVICEREMOVEPENDING         = 0x8003;  // about to remove, still avail.
const DBT_DEVICEREMOVECOMPLETE        = 0x8004;  // device is gone
const DBT_DEVICETYPESPECIFIC          = 0x8005;  // type specific event

//copy plugin's status
const	TST_Idel = 0x1010;
const	TST_Copying = 0x2020;
const	TST_Over   = 0x3030;
const	TST_Cancel = 0x4040;
const	TST_Failed = 0x8080;
const	TST_Failed_DeviceLocked = 0x8081;
const	TST_Failed_DeviceFull = 0x8082;

// Key control mode
const KEY_CTRL_MODE_PLAY = 0;
const KEY_CTRL_MODE_NAVIGATE = 1;

//disc Type
const DISC_TYPE_NOMEDIA = 0;
const DISC_TYPE_DVD = 1;
const DISC_TYPE_VCD = 2;
const DISC_TYPE_SVCD = 3;
const DISC_TYPE_CD = 4;
const DISC_TYPE_UNKNOWN = 5;
