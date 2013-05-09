#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#include <string.h>
#include "xprintf.h"

// Languages
#define LANG_DUTCH 0
#define LANG_ENGLISH 1
#define LANG_FRENCH 2
#define LANG_GERMAN 3
#define LANG_SPANISH 4
#define LANG_MAX 5

// Non Working Days Country
#define NWD_NONE 0
#define NWD_FRANCE 1
#define NWD_MAX 2

// Compilation-time options
#define LANG_CUR LANG_FRENCH
#define NWD_COUNTRY NWD_FRANCE
#define WEEK_STARTS_ON_SUNDAY false
#define SHOW_WEEK_NUMBERS true

#if LANG_CUR == LANG_DUTCH
#define APP_NAME "Kalender"
#elif LANG_CUR == LANG_FRENCH
#define APP_NAME "Calendrier"
#elif LANG_CUR == LANG_GERMAN
#define APP_NAME "Kalender"
#elif LANG_CUR == LANG_SPANISH
#define APP_NAME "Calendario"
#else // Defaults to English
#define APP_NAME "Calendar"
#endif

static const char *windowName = APP_NAME;

static const char *monthNames[] = {
#if LANG_CUR == LANG_DUTCH
	"Januari", "Februari", "Maart", "April", "Mei", "Juni", "Juli", "Augustus", "September", "Oktober", "November", "December"
#elif LANG_CUR == LANG_FRENCH
	"Janvier", "Février", "Mars", "Avril", "Mai", "Juin", "Juillet", "Août", "Septembre", "Octobre", "Novembre", "Décembre"
#elif LANG_CUR == LANG_GERMAN
	"Januar", "Februar", "März", "April", "Mai", "Juni", "Juli", "August", "September", "Oktober", "November", "Dezember"
#elif LANG_CUR == LANG_SPANISH
	"Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio", "Julio", "Augusto", "Septiembre", "Octubre", "Noviembre", "Diciembre"
#else // Defaults to English
	"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"
#endif
};

static const char *weekDays[] = {
#if LANG_CUR == LANG_DUTCH
	"Zo", "Ma", "Di", "Wo", "Do", "Vr", "Za"	// Dutch
#elif LANG_CUR == LANG_FRENCH
	"Di", "Lu", "Ma", "Me", "Je", "Ve", "Sa"	// French
#elif LANG_CUR == LANG_GERMAN
	"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"	// German
#elif LANG_CUR == LANG_SPANISH
	"Do", "Lu", "Ma", "Mi", "Ju", "Vi", "Sá"	// Spanish
#else // Defaults to English
	"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"	// English
#endif
};

#define MY_UUID { 0x38, 0xE4, 0xB1, 0xAB, 0x47, 0x4E, 0x4F, 0xEC, 0xBD, 0x42, 0xDB, 0xD2, 0x20, 0xC6, 0xCD, 0x51 }
PBL_APP_INFO(MY_UUID,
			 APP_NAME, "Jnm",
			 1, 1, /* App version */
			 RESOURCE_ID_IMAGE_MENU_ICON,
			 APP_INFO_STANDARD_APP);

#define SCREENW 144
#define SCREENH 168
#define MENUBAR_HEIGHT 16
#define MONTHNAME_LAYER_HEIGHT 20
#define MONTH_LAYER_HEIGHT (SCREENH-MENUBAR_HEIGHT-MONTHNAME_LAYER_HEIGHT)

typedef struct {
	int day;
	int month;
	int year;
} Date;

Window window;
Layer monthLayer;
TextLayer monthNameLayer;
Date today;
int displayedMonth, displayedYear;
//GFont myFont;

static int DX, DY, DW, DH;

#define NUM_NON_WORKING_DAYS 11
typedef struct {
	char name[30];
	Date date;
} nonWorkingDay;

#if NWD_COUNTRY == NWD_FRANCE
nonWorkingDay nonWorkingDays[NUM_NON_WORKING_DAYS];
#endif

static char monthName[40] = "";

static int julianDay(const Date *theDate) {
	int a = (int)((13-theDate->month)/12);
	int y = theDate->year+4800-a;
	int m = theDate->month + 12*a - 2;
	
	int day = theDate->day + (int)((153*m+2)/5) + y*365 + (int)(y/4) - (int)(y/100) + (int)(y/400) - 32045;
	return day;
}

static int dayOfWeek(const Date *theDate) {
	int J = julianDay(theDate);
	return (J+1)%7;
}
#if SHOW_WEEK_NUMBERS
static int weekNumber(const Date *theDate) {
	int J = julianDay(theDate);
	
	int d4 = (J+31741-(J%7))%146097%36524%1461;
	int L = (int)(d4/1460);
	int d1 = ((d4-L)%365)+L;
	
	return (int)(d1/7)+1;
}
#endif

static bool isLeapYear(const int Y) {
	return (((Y%4 == 0) && (Y%100 != 0)) || (Y%400 == 0));
}

static int numDaysInMonth(const int M, const int Y) {
	static const int nDays[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
	
	return nDays[M] + (M == 1)*isLeapYear(Y);
}

#define Date(d, m, y) ((Date){ (d), (m), (y) })

static void dateAddDays(Date *date, int numDays) {
	int i;
	
	for (i=0; i<numDays; i++) {
		if (date->day == numDaysInMonth(date->month, date->year)) {
			if (date->month == 11) {
				date->year++;
				date->month = 0;
				date->day = 1;
			} else {
				date->month++;
				date->day = 1;
			}
		} else {
			date->day++;
		}
	}
}

static int compareDates(Date *d1, Date *d2) {
	if (d1->year < d2->year) {
		return -1;
	} else if (d1->year > d2->year) {
		return 1;
	} else {
		if (d1->month < d2->month) {
			return -1;
		} else if (d1->month > d2->month) {
			return 1;
		} else {
			if (d1->day < d2->day) {
				return -1;
			} else if (d1->day > d2->day) {
				return 1;
			} else {
				return 0;
			}
		}
	}
}

#if NWD_COUNTRY == NWD_FRANCE
static void sortNonWorkingDays() {
	int i, j;
	nonWorkingDay t;
	
	for (i=0; i<NUM_NON_WORKING_DAYS-1; i++) {
		for (j=i+1; j<NUM_NON_WORKING_DAYS; j++) {
			if (compareDates(&nonWorkingDays[j].date, &nonWorkingDays[i].date) < 0) {
				t = nonWorkingDays[i];
				nonWorkingDays[i] = nonWorkingDays[j];
				nonWorkingDays[j] = t;
			}
		}
	}
}

static void easterMonday(const int Y, Date *theDate) {
	int a = Y-(int)(Y/19)*19;
	int b = (int)(Y/100);
	int C = Y-(int)(Y/100)*100;
	int P = (int)(b/4);
	int E = b-(int)(b/4)*4;
	int F = (int)((b + 8) / 25);
	int g = (int)((b - F + 1) / 3);
	int h = (19 * a + b - P - g + 15)-(int)((19 * a + b - P - g + 15)/30)*30;
	int i = (int)(C / 4);
	int K = C-(int)(C/4)*4;
	int r = (32 + 2 * E + 2 * i - h - K) - (int)((32 + 2 * E + 2 * i - h - K)/7)*7;
	int N = (int)((a + 11 * h + 22 * r) / 451);
	int M = (int)((h + r - 7 * N + 114) / 31);
	int D = ((h + r - 7 * N + 114)-(int)((h + r - 7 * N + 114)/31)*31) + 1;
	
	if (D == numDaysInMonth(M-1, Y)) {
		theDate->day = 1;
		theDate->month = M;
	} else {
		theDate->day = D+1;
		theDate->month = M-1;
	}
	theDate->year = Y;
}

static void ascensionDay(const int Y, Date *theDate) {
	easterMonday(Y, theDate);
	dateAddDays(theDate, 38);
}

static void whitMonday(const int Y, Date *theDate) {
	easterMonday(Y, theDate);
	dateAddDays(theDate, 49);
}

void computeNonWorkingDays(const int Y) {
	nonWorkingDay *day = nonWorkingDays;
	
	xsprintf(day->name, "Jour de l'an");
	day->date.day = 1;  day->date.month = 0;  day->date.year = Y; day++; // Jour de l'an
	xsprintf(day->name, "Fête du travail");
	day->date.day = 1;  day->date.month = 4;  day->date.year = Y; day++; // Fête du travail
	xsprintf(day->name, "Armistice 1945");
	day->date.day = 8;  day->date.month = 4;  day->date.year = Y; day++; // Armistice 1945
	xsprintf(day->name, "Fête Nationale");
	day->date.day = 14; day->date.month = 6;  day->date.year = Y; day++; // Fête nationale
	xsprintf(day->name, "Assomption");
	day->date.day = 15; day->date.month = 7;  day->date.year = Y; day++; // Assomption
	xsprintf(day->name, "Toussaint");
	day->date.day = 1;  day->date.month = 10; day->date.year = Y; day++; // Toussaint
	xsprintf(day->name, "Armistice 1918");
	day->date.day = 11; day->date.month = 10; day->date.year = Y; day++; // Armistice 1918
	xsprintf(day->name, "Noël");
	day->date.day = 25; day->date.month = 11; day->date.year = Y; day++; // Noël
	xsprintf(day->name, "Lundi de Pâques");
	easterMonday(Y, &day->date); day++;
	xsprintf(day->name, "Jeudi de l'Ascension");
	ascensionDay(Y, &day->date); day++;
	xsprintf(day->name, "Lundi de Pentecôte");
	whitMonday(Y, &day->date);
	
	sortNonWorkingDays();
}

static bool isNonWorkingDay(const Date *theDate) {
	Date d;
	
	if (theDate->day == 1  && theDate->month ==  0) return true; // Jour de l'an
	if (theDate->day == 1  && theDate->month ==  4) return true; // Fête du travail
	if (theDate->day == 8  && theDate->month ==  4) return true; // Armistice 1945
	if (theDate->day == 14 && theDate->month ==  6) return true; // Fête nationale
	if (theDate->day == 15 && theDate->month ==  7) return true; // Assomption
	if (theDate->day == 1  && theDate->month == 10) return true; // Toussaint
	if (theDate->day == 11 && theDate->month == 10) return true; // Armistice 1918
	if (theDate->day == 25 && theDate->month == 11) return true; // Noël
	
	easterMonday(theDate->year, &d);
	if (theDate->day == d.day && theDate->month == d.month) return true; // Lundi de Pâques
	ascensionDay(theDate->year, &d);
	if (theDate->day == d.day && theDate->month == d.month) return true; // Jeudi de l'ascension
	whitMonday(theDate->year, &d);
	if (theDate->day == d.day && theDate->month == d.month) return true; // Lundi de Pentecôte

	return false;
}
#else // NWD_COUNTRY == NWD_FRANCE
static bool isNonWorkingDay(const Date *theDate) {
	return false;
}
#endif //  NWD_COUNTRY == NWD_FRANCE


void updateMonthText() {
	xsprintf(monthName, "%s %d", monthNames[displayedMonth], displayedYear);
	text_layer_set_text(&monthNameLayer, monthName);
}

void updateMonth(Layer *layer, GContext *ctx) {
	static char numStr[3] = "";
	int i, x, s, numWeeks, dy, firstday, numDays, l=0, c=0, w;
	Date first, d;
	GFont f, fontNorm, fontBold;
	GRect rect, fillRect;
	
	first = Date(1, displayedMonth, displayedYear);
#if WEEK_STARTS_ON_SUNDAY
	firstday = dayOfWeek(&first);
#else
	firstday = (dayOfWeek(&first)+6)%7;
#endif
	numDays = numDaysInMonth(displayedMonth, displayedYear);
	
	numWeeks = (firstday+6+numDays)/7;
	
	dy = DY + DH*(6-numWeeks)/2;
	
	graphics_context_set_stroke_color(ctx, GColorBlack);
	graphics_context_set_fill_color(ctx, GColorBlack);
	
	// Calendar Grid
#if SHOW_WEEK_NUMBERS
	x = DX+DW;
#else
	x = DX;
#endif

	// Black Top Line with days of week
	graphics_fill_rect(ctx, GRect(x, dy, DW*7+1, DH), 4, GCornersTop);

#if SHOW_WEEK_NUMBERS
	// Black left column for week numbers
	graphics_fill_rect(ctx, GRect(DX, dy+DH, DW, numWeeks*DH+1), 4, GCornersLeft);
#endif

#if SHOW_WEEK_NUMBERS
	x = DX+DW;
	w = DW*7;
#else
	x = DX+1;
	w = DW*7-1;
#endif
	// Double line on the outside
	graphics_draw_round_rect(ctx, GRect(x, dy+DH, w, numWeeks*DH), 0);
	
	// Column(s) for the week-end or sunday
#if WEEK_STARTS_ON_SUNDAY
	x = DX+DW+1;
#else
	x = DX+5*DW+1;
#endif

#if SHOW_WEEK_NUMBERS
	x += DW;
#endif

	graphics_draw_line(ctx, GPoint(x, dy+DH), GPoint(x, dy+DH+numWeeks*DH-1));
	
#if SHOW_WEEK_NUMBERS
	x = 1;
#else
	x = 0;
#endif

	// Vertical lines
	for (i=x; i<=x+7; i++) {
		graphics_draw_line(ctx, GPoint(DX+DW*i,dy+DH), GPoint(DX+DW*i,dy+(numWeeks+1)*DH));
	}
	// Horizontal lines
	for (i=1; i<=(numWeeks+1); i++) {
		graphics_draw_line(ctx, GPoint(DX+x*DW,dy+DH*i), GPoint(DX+DW*(7+x),dy+DH*i));
	}
	
	fontNorm = fonts_get_system_font(FONT_KEY_GOTHIC_18);
	fontBold = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
	f = fontNorm;
	
#if WEEK_STARTS_ON_SUNDAY
	s = 0;
#else
	s = 1;
#endif

#if SHOW_WEEK_NUMBERS
	x = 1;
#else
	x = 0;
#endif

	// Days of week
	graphics_context_set_text_color(ctx, GColorWhite);
	
	for (i=s; i<s+7; i++) {
		graphics_text_draw(ctx, weekDays[i%7], fonts_get_system_font(FONT_KEY_GOTHIC_14), GRect(DX+DW*(i+x-s), dy, DW+2, DH+1), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
	}
	
#if SHOW_WEEK_NUMBERS
	// Week numbers
	for (i=0, d=first; i<=numWeeks; i++, d.day+=7) {
		xsprintf(numStr, "%d", weekNumber(&d));
		graphics_text_draw(ctx, numStr, fonts_get_system_font(FONT_KEY_GOTHIC_14), GRect(DX, dy+DH*(i+1), DW, DH+1), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
	}
#endif
	
	// Day numbers
	graphics_context_set_text_color(ctx, GColorBlack);
	
	for (i=1; i<=numDays; i++) {
		c = (firstday - 1 + i)%7;
		if (c == 0 && i != 1) {
			l++;
		}
		
		xsprintf(numStr, "%d", i);

		if (isNonWorkingDay(&Date(i, displayedMonth, displayedYear))) {
			f = fontBold;
		} else {
			f = fontNorm;
		}
		
		fillRect = GRect(DX+DW*(c+x), dy+DH*(l+1), DW, DH);
		rect = GRect(DX+DW*(c+x), dy+DH*(l+1)-3, DW+1, DH+1);
		
		if (today.day == i && today.month == displayedMonth && today.year == displayedYear) {
			graphics_fill_rect(ctx, fillRect, 0, GCornerNone);
			graphics_context_set_text_color(ctx, GColorWhite);
			graphics_text_draw(ctx, numStr, f, rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
		} else {
			graphics_context_set_text_color(ctx, GColorBlack);
			graphics_text_draw(ctx, numStr, f, rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
		}
	}
}

static int numClicks = 0;
void btn_up_handler(ClickRecognizerRef recognizer, Window *window) {
	numClicks = 0;
}

void up_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
	numClicks++;
	if (numClicks < 25) {
		displayedMonth--;
		if (displayedMonth == -1) {
			displayedMonth = 11;
			displayedYear--;
		}
	} else {
		displayedYear--;
	}
	
	updateMonthText();
	layer_mark_dirty(&monthLayer);
}


void down_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
	numClicks++;
	if (numClicks < 25) {
		displayedMonth++;
		if (displayedMonth == 12) {
			displayedMonth = 0;
			displayedYear++;
		}
	} else {
		displayedYear++;
	}
	
	updateMonthText();
	layer_mark_dirty(&monthLayer);
	
}

void select_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
	displayedMonth = today.month;
	displayedYear = today.year;
	
	updateMonthText();
	layer_mark_dirty(&monthLayer);	
}

void click_config_provider(ClickConfig **config, Window *window) {
	config[BUTTON_ID_SELECT]->click.handler = (ClickHandler) select_single_click_handler;
	
	config[BUTTON_ID_UP]->click.handler = (ClickHandler) up_single_click_handler;
	config[BUTTON_ID_UP]->raw.up_handler = (ClickHandler) btn_up_handler;
	config[BUTTON_ID_UP]->click.repeat_interval_ms = 100;
	
	config[BUTTON_ID_DOWN]->click.handler = (ClickHandler) down_single_click_handler;
	config[BUTTON_ID_DOWN]->raw.up_handler = (ClickHandler) btn_up_handler;
	config[BUTTON_ID_DOWN]->click.repeat_interval_ms = 100;
}

void handle_tick(AppContextRef ctx, PebbleTickEvent *t) {
	updateMonthText();
	layer_mark_dirty(&monthLayer);
}

void handle_init(AppContextRef ctx) {
	PblTm now;
	
	window_init(&window, windowName);
	window_stack_push(&window, true /* Animated */);

	resource_init_current_app(&APP_RESOURCES);

	get_time(&now);
	today.day = now.tm_mday;
	today.month = now.tm_mon;
	today.year = now.tm_year + 1900;
	
	displayedMonth = today.month;
	displayedYear = today.year;
	
#if SHOW_WEEK_NUMBERS
	DW = (SCREENW-2)/8;
	DX = 1 + (SCREENW-2 - 8*DW)/2;
#else
	DW = (SCREENW-2)/7;
	DX = 1 + (SCREENW-2 - 7*DW)/2;
#endif
	DH = MONTH_LAYER_HEIGHT/7;
	DY = (MONTH_LAYER_HEIGHT - 7*DH)/2;

	//myFont = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_15));

	text_layer_init(&monthNameLayer, GRect(0, 0, SCREENW, MONTHNAME_LAYER_HEIGHT));
	text_layer_set_background_color(&monthNameLayer, GColorWhite);
	text_layer_set_text_color(&monthNameLayer, GColorBlack);
	text_layer_set_font(&monthNameLayer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	text_layer_set_text_alignment(&monthNameLayer, GTextAlignmentCenter);
	layer_add_child(&window.layer, &monthNameLayer.layer);
	
	updateMonthText();
	
	layer_init(&monthLayer, GRect(0, MONTHNAME_LAYER_HEIGHT, SCREENW, MONTH_LAYER_HEIGHT));
	layer_set_update_proc(&monthLayer, &updateMonth);
	layer_add_child(&window.layer, &monthLayer);
	
	window_set_click_config_provider(&window, (ClickConfigProvider) click_config_provider);
}

void handle_deinit(AppContextRef ctx) {
	//fonts_unload_custom_font(myFont);
}

void pbl_main(void *params) {
	PebbleAppHandlers handlers = {
		.init_handler = &handle_init,
		.deinit_handler = &handle_deinit,
		
		.tick_info = {
			.tick_handler = &handle_tick,
			.tick_units = DAY_UNIT
		}
	};
	app_event_loop(params, &handlers);
}
