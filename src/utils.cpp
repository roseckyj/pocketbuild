static void menu_01_handler(int index)
{
	char buffer[2048];
	snprintf(buffer, sizeof(buffer), "Menu: index %d has been selected!", index);
	Message(ICON_INFORMATION, "Debug", buffer, 3*1000);
}

static void menu_01()
{
	static imenu new_game_menu[] = {
        { ITEM_ACTIVE, 111, "One player", NULL },
        { ITEM_ACTIVE, 112, "Two players", NULL },
        
        { 0, 0, NULL, NULL }
    };

    static imenu difficulty_menu[] = {
        { ITEM_ACTIVE, 121, "Easy", NULL },
        { ITEM_BULLET, 122, "Normal", NULL },
        { ITEM_ACTIVE, 123, "Hard", NULL },
        
        { 0, 0, NULL, NULL }
    };

    static imenu main_menu[] = {
        { ITEM_HEADER, 0, "Menu", NULL },
        { ITEM_ACTIVE, 101, "Resume", NULL },
        { ITEM_SUBMENU, 110, "New Game", new_game_menu },
        { ITEM_SUBMENU, 120, "Difficulty", difficulty_menu },
        { ITEM_ACTIVE, 140, "About", NULL },
        { ITEM_ACTIVE, 150, "Exit", NULL },
        
        { 0, 0, NULL, NULL }
    };

	OpenMenu(main_menu, 0, 100, 200, (iv_menuhandler)menu_01_handler);
}