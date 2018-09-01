#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <ucr/io.c>
#include <ucr/timer.h>
#include <ucr/scheduler.h>

#define left  0x04 
#define down  0x08
#define up    0x10
#define right 0x20
#define shooter_up 0x02
#define shooter_down 0x01
#define start_xpos  2
#define start_ypos  0
#define shooter_start 16
#define easy_cursor_pos 18
#define med_cursor_pos  23
#define hard_cursor_pos 28
#define yes_cursor_pos 26
#define no_cursor_pos  30
#define score_cursor_pos 7
#define best_cursor_pos 15
#define easy 0
#define med 1
#define hard 2
#define sp ' '	
#define bullet '-'									
#define wall 219
#define enemy 248
#define w 219											// wall alias
#define x  248											// enemy alias
#define arrow 126
#define screen_size 16
#define MAX_SCORE 60
#define begin_of_map 0
#define len_map  38
#define custom_player 0
#define custom_enemy 1
#define custom_shot_enemy 2
#define custom_wall 3
#define custom_shooter 4
#define win 5
#define len_mode_msg 4
#define checkL map_arr[player_ypos][player_xpos + map_pos - 2]	
#define checkR map_arr[player_ypos][player_xpos + map_pos]	
#define checkD map_arr[1][player_xpos + map_pos - 1] 
#define checkU map_arr[0][player_xpos + map_pos - 1] 
#define checkPlayerPos map_arr[player_ypos][player_xpos + map_pos - 1]
#define shot_curr map_arr[shot_ypos][map_pos + shot_xpos - 1]
#define player_shot_x shot_xpos == player_xpos
#define player_shot_y shot_ypos == player_ypos


int ResetTick(int state), CheckPauseTick(int state), DisplayMapTick(int state);
int MovePlayerTick(int state), MenuTick(int state), ScrollTick(int state), TrackShotsTick(int state);
int BlinkArrowTick(int state), VictoryTick(int state), ScoreTick(int state), ShooterTick(int state);
void DisplayMap(), DisplayStartMsg(), DisplayModeMsg(), DisplayPlayer(), DisplayWinnerMsg(), DisplayShooter();
void MovePlayerLeft(), MovePlayerRight(), UpdateModeArrow(), ClearPlayer(), ClearShooter(), MoveAndCheckBullets();
void ShootBullet();
unsigned char player_xpos, moved_map, moved_player, player_ypos, arrow_pos;
unsigned char shooter_pos, moved_shooter, shoot_enabled;
unsigned char paused, defeated, winner, reset, play_again, score, read_score; 
unsigned char in_menu, game_mode, scroll_speed, scroll_count, i;
unsigned char map_pos, curr_pos;
signed char shot_xpos;
unsigned char shot_ypos;
const unsigned char* loser_msg = "You are NOT a   winner this time";
const unsigned char* title_msg = "-Speed  Runner-" ;
const unsigned char* press_start_msg = "Press Start";
const unsigned char* mode_msg = "Choose Mode:   Easy Hard VHard";
const unsigned char* winner_msg = "Score:   Best:  Continue? Yes No";
const unsigned char* gameover_msg = "Game Over...";
const unsigned char player_pattern[8] = { 0x00, 0x0F, 0x1F, 0x10, 0x05, 0x10, 0x0F, 0x05 }; 
const unsigned char enemy_pattern[8] = { 0x1F, 0x0A, 0x11, 0x00, 0x11, 0x0A, 0x1F, 0x1F };
const unsigned char shot_enemy_pattern[8] = {  0x1F, 0x00, 0x11, 0x1F, 0x04, 0x0A, 0x11, 0x00 };
const unsigned char wall_pattern[8] = { 0x00, 0x1F, 0x11, 0x1F, 0x11, 0x11, 0x1F, 0x00 };
const unsigned char shooter_pattern[8] = { 0x00, 0x1E, 0x1F, 0x01, 0x14, 0x01, 0x1E, 0x14 };
const unsigned char win_pattern[8] = { 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F };
const unsigned char modes_pos [3] = { easy_cursor_pos, med_cursor_pos, hard_cursor_pos };
const unsigned char map_arr[2][37]  = { 
		{w,  sp, sp, sp, sp, sp, sp, sp,  x, sp, sp, sp,  w, sp, sp, sp, sp,  x,  x, sp, sp, sp,  x,  x,  x,  w, sp, sp, sp,  x, sp, sp, sp,  x,  x, win, x},
		{w, sp, sp, sp, sp, sp,  x, sp, sp, sp,  x, sp, sp, sp,  x,  x, sp, sp, sp, sp,  w, sp, sp, sp, sp, sp, sp,  x, sp, sp, sp,  x, sp, sp, sp,  sp, x}
	};


enum MapToDisplay { init_display, ASSIGN_display, wait_display, DISPLAY } display_state;
enum CheckPause { ASSIGN_pause, init_pause, init_pausex, CHECK, PAUSEx, RESUME } pause_state;
enum MovePlayer { init_vert, wait_vert, MOVE_UP_p, MOVE_DW_p, MOVE_LT, MOVE_LTx, MOVE_RT, MOVE_RTx } vert_state;
enum Menu { init_menu, init_menux, TITLE, TITLEx, MODE, MODEx } menu_state;
enum SoftReset { init_reset, ASSIGN_reset, wait_reset } reset_state;
enum Scroll { init_scroll, wait_scroll, SCROLL } scroll_state;
enum Victory { init_victory, wait_victory, VICTORY, VICTORYx, GAME_OVER } victory_state;
enum Blink { init_blink, wait_blink, BLINK_ON, BLINK_OFF } blink_state;
enum Score { ASSIGN_score, init_score, wait_score, PLAYING } score_state;
enum Shooter { ASSIGN_shooter, init_shooter, wait_shooter, MOVE_UP_s, MOVE_DW_s, SHOOT, SHOOTx } shooter_state;
enum MonitorShots { wait_track, MONITOR } track_state;


int main(void)
{
	DDRA = 0x00; PORTA = 0xFF;
	DDRB = 0xFF; PORTB = 0x00;
	DDRC = 0x00; PORTC = 0xFF;
	DDRD = 0xFF; PORTD = 0x00;

	static task display_task, vert_task, pause_task, menu_task, reset_task, scroll_task, victory_task, blink_task, score_task, shooter_task, track_task;
	task *tasks[] = { &display_task, &pause_task, &vert_task, &menu_task, &reset_task, &scroll_task, &victory_task, &blink_task, &score_task, &shooter_task, &track_task };
	const unsigned short numTasks = sizeof(tasks)/sizeof(task*);
	
	unsigned long display_task_calc = 100;
	unsigned long pause_task_calc = 50;
	unsigned long vert_task_calc = 50;
	unsigned long menu_task_calc = 50;
	unsigned long reset_task_calc = 300;
	unsigned long scroll_state_calc = 200;
	unsigned long victory_task_calc = 50;
	unsigned long blink_task_calc = 400;
	unsigned long score_task_calc = 1000;
	unsigned long shooter_task_calc = 50;
	unsigned long track_task_calc = 50;

	unsigned long int GCD = findGCD(display_task_calc, pause_task_calc);
	GCD = findGCD(GCD, vert_task_calc);
	GCD = findGCD(GCD, menu_task_calc);
	GCD = findGCD(GCD, reset_task_calc);
	GCD = findGCD(GCD, scroll_state_calc);
	GCD = findGCD(GCD, victory_task_calc);
	GCD = findGCD(GCD, blink_task_calc);
	GCD = findGCD(GCD, score_task_calc);
	GCD = findGCD(GCD, shooter_task_calc);
	GCD = findGCD(GCD, track_task_calc);

	unsigned long display_task_period = display_task_calc/GCD;
	unsigned long vert_task_period = vert_task_calc/GCD;
	unsigned long pause_task_period = pause_task_calc/GCD;
	unsigned long menu_task_period = menu_task_calc/GCD;
	unsigned long reset_task_period = reset_task_calc/GCD;
	unsigned long scroll_task_period = scroll_state_calc/GCD;
	unsigned long victory_task_period = victory_task_calc/GCD;
	unsigned long blink_task_period = blink_task_calc/GCD;
	unsigned long score_task_period = score_task_calc/GCD;
	unsigned long shooter_task_period = shooter_task_calc/GCD;
	unsigned long track_task_period = track_task_calc/GCD;

	reset_task.state = -1;
	reset_task.period = reset_task_period;
	reset_task.elapsedTime = reset_task_period;
	reset_task.TickFct = &ResetTick;
	
	menu_task.state = -1;
	menu_task.period = menu_task_period;
	menu_task.elapsedTime = menu_task_period;
	menu_task.TickFct = &MenuTick;
	
	display_task.state = -1;
	display_task.period = display_task_period;
	display_task.elapsedTime = display_task_period;
	display_task.TickFct = &DisplayMapTick;
	
	pause_task.state = -1;
	pause_task.period = pause_task_period;
	pause_task.elapsedTime = pause_task_period;
	pause_task.TickFct = &CheckPauseTick;

	vert_task.state = -1;
	vert_task.period = vert_task_period;
	vert_task.elapsedTime = vert_task_period;
	vert_task.TickFct = &MovePlayerTick;
	
	scroll_task.state = -1;
	scroll_task.period = scroll_task_period;
	scroll_task.elapsedTime = scroll_task_period;
	scroll_task.TickFct = &ScrollTick;

	victory_task.state = -1;
	victory_task.period = victory_task_period;
	victory_task.elapsedTime = victory_task_period;
	victory_task.TickFct = &VictoryTick;

	blink_task.state = -1;
	blink_task.period = blink_task_period;
	blink_task.elapsedTime = blink_task_period;
	blink_task.TickFct = &BlinkArrowTick;

	score_task.state = -1;
	score_task.period = score_task_period;
	score_task.elapsedTime = score_task_period;
	score_task.TickFct = &ScoreTick;
	
	shooter_task.state = -1;
	shooter_task.period = shooter_task_period;
	shooter_task.elapsedTime = shooter_task_period;
	shooter_task.TickFct = &ShooterTick;

	track_task.state = -1;
	track_task.period = track_task_period;
	track_task.elapsedTime = track_task_period;
	track_task.TickFct = &TrackShotsTick;

	LCD_init();
	TimerSet(GCD);
	TimerOn();
	
	while(1) {
		// Task Scheduler
		for (unsigned short i = 0; i < numTasks; i++ ) {
			if ( tasks[i]->elapsedTime == tasks[i]->period ) {
				tasks[i]->state = tasks[i]->TickFct(tasks[i]->state);
				tasks[i]->elapsedTime = 0;
			}
			tasks[i]->elapsedTime += 1;
		}
		while(!TimerFlag);
		TimerFlag = 0;
	}
}


// Display current section of map if finished with menu and moved map_pos
int DisplayMapTick(int state) {
	switch (state){
		case ASSIGN_display:
			state = init_display;
		
		case init_display:		
			if ( !(in_menu || winner) )		
				state = wait_display;
			break;
					
		case wait_display:	
			if ( reset || defeated )
				state = ASSIGN_display;
			else
				state = (moved_map) ? DISPLAY : wait_display;
			break;
		
		case DISPLAY:   
			state = (moved_map) ? DISPLAY : wait_display;
			break;
		
		default:
			state = ASSIGN_display;
			break;
	}
	switch (state){
		case ASSIGN_display:
			moved_map = 1;
			moved_player = 1;
			map_pos = 0; 
			player_xpos = start_xpos;
			player_ypos = start_ypos;
			break;
		
		case init_display:       
			break;
		
		case wait_display:
			break;
		
		case DISPLAY:        
			// Clear old display and re-draw according to changes made
			LCD_ClearScreen();		
			DisplayMap();
			DisplayPlayer();
			DisplayShooter();

			// Don't re-draw until a change has been made, move cursor away
			moved_map = 0;
			LCD_Cursor(0);
			break;
		
		default:
			break;
	}
	return state;
}


// Control Cursor Position (Player position) 
// Move Right, Left are more complicated because they can scroll   
int MovePlayerTick(int state) {
	unsigned char direction = ~PINA & 0x3C;
	unsigned char pause_button = ~PINA & 0x40;
	switch (state){
		case init_vert:
			if (!in_menu)
				state = wait_vert;
			break;
		
		case wait_vert:            // Only move player if no pause
			if (reset || in_menu)
				state = init_vert;
			else if ( !(paused || pause_button) ) {
				if (direction == down)
					state = MOVE_DW_p;
				else if (direction == left)
					state = MOVE_LT;
				else if (direction == right)
					state = MOVE_RT;
				else
					state = (direction == up) ? MOVE_UP_p : wait_vert;
			}
			break;
		
		case MOVE_UP_p:           
			state = wait_vert;
			break;
		
		case MOVE_DW_p:
			state = wait_vert; 
			break;
		
		case MOVE_RT:
			state = MOVE_RTx;
			break;

		case MOVE_RTx:
			if (direction != right)
				state = wait_vert;
			break;	

		case MOVE_LT:
			state = MOVE_LTx;
			break;
		
		case MOVE_LTx:
			if (direction != left)
				state = wait_vert;
			break;	

		default:
			state = init_vert;	
			break;
	}	
	switch (state){
		case init_vert:
			break;
		
		case wait_vert:
			if ( !(defeated || winner) && moved_player ){
				DisplayPlayer();
			}
			break;
		
		case MOVE_UP_p:	// If (on_bot) and (no_wall_above), move top  
			if ( (player_ypos) && (checkU != wall) ) {
				ClearPlayer();
				player_ypos = 0;
				moved_player = 1;	
			}
			break;
		
		case MOVE_DW_p:	// If (on_top) and (no_wall_below), move bot
			if ( (!player_ypos) && (checkD != wall)){
				ClearPlayer();
				player_ypos = 1;
				moved_player = 1;
			}
			break;
		
		case MOVE_LT:		// Move left if (no_wall_left) and (xpos>0)
			MovePlayerLeft();
			break;

		case MOVE_LTx:
			break;
		
		case MOVE_RT:	// Move right if (no_wall_right) && (xpos>mapsize-screensize)
			MovePlayerRight();
			break;
	
		case MOVE_RTx:
			break;
	
		default:
			break;
	}
	return state;
}				   


// Handle pause_button and cases that affect defeat/win/pause flags
int CheckPauseTick(int state) {
	unsigned char pause_button = ~PINA & 0x40;
	switch (state){		
		
		case ASSIGN_pause:
			state = init_pause;
			break;
			
		case init_pause:
			if (!in_menu)		
				state = init_pausex;
			break;

		case init_pausex:	
			state = (pause_button) ? init_pausex : CHECK;	
			break;		

		case CHECK:
			if (reset)
				state = ASSIGN_pause;
			else
				state = (paused) ? PAUSEx : CHECK;
			break;
		
		case PAUSEx:
			state = (pause_button) ? PAUSEx : RESUME;
			break;
		
		case RESUME:		// Adjust for win and menu
			state = (pause_button) ? ASSIGN_pause : RESUME;
			break;
		
		default:
			state = ASSIGN_pause;
			break;
	}
	switch (state){		
		case ASSIGN_pause:
			paused = 0;
			defeated = 0;
			break;
		
		case init_pause:
			break;

		case init_pausex:
			break;
		
		case CHECK:
			curr_pos = checkPlayerPos;
			if ( player_shot_x && player_shot_y ) {	// Dead
    			paused = 1;
    			defeated = 1;
    			LCD_DisplayString(1, loser_msg);
			}
			if (curr_pos == enemy) {				// Dead
				paused = 1;
				defeated = 1;
				LCD_DisplayString(1, loser_msg);
				LCD_Cursor(0);
			}
			else if (curr_pos == win) {				// Win
				defeated = 1;					
				paused = 1;
				winner = 1;				
			}
			if (pause_button)
				paused = 1;
				
			break;

		case PAUSEx:
			break;
		
		case RESUME:
			break;
		
		default:
			break;
	}
	return state;
}


// Handle menu and everything that is displayed, AND coming in from reset (Mealy)
// or if game won. Easy mode is 0ms scroll speed, Hard -> 200ms, VHArd-> 400ms
int MenuTick(int state) {
	unsigned char start_button = ~PINA & 0x40;
	unsigned char direction = ~PINA & 0x24;
	
	switch (state){
		case init_menu:
			if (in_menu) {	
				state = init_menux;
				if (!play_again)
					DisplayStartMsg();
			}
			break;

		case init_menux:		
			if (!start_button || play_again)
				state = TITLE;
			break;
		
		case TITLE:
			if ( (start_button && !reset) || (play_again) ) {
				state = TITLEx;
				LCD_DisplayString(3, mode_msg);
				game_mode = easy;
				arrow_pos = modes_pos[game_mode] - 1;
			}
			break;
		
		case TITLEx:
			state = (start_button || direction) ? TITLEx : MODE;
			break;
		
		case MODE:
			if (reset)
				state = init_menu;
			else if (direction) {		
				state = TITLEx;		// ensure only one movement at a time
				if (direction == left) {
    				game_mode = (game_mode == easy) ? hard : game_mode-1;
    				UpdateModeArrow();
				}
				else if (direction == right) {
    				game_mode = (game_mode == hard) ? easy : game_mode+1;
    				UpdateModeArrow();
				}
			}
			else if (start_button) {
					state = MODEx;
					arrow_pos = 0;
				}
			else
				state = MODE;
			break;
		
		case MODEx:
			if (start_button) {
				state = init_menu;	
				in_menu = 0;
			}
			break;
		
		default:
			// Create custom characters once
 			LCD_Custom_Char(custom_player, player_pattern);	
 			LCD_Custom_Char(custom_enemy, enemy_pattern);
 			LCD_Custom_Char(custom_shot_enemy, shot_enemy_pattern);
 			LCD_Custom_Char(custom_wall, wall_pattern);
 			LCD_Custom_Char(custom_shooter, shooter_pattern);
			LCD_Custom_Char(win, win_pattern);
			state = init_menu;
			break;
	}
	return state;
}


// Blink Arrow in menu and continue screens
int BlinkArrowTick(int state) {
    switch (state){
		case init_blink:
			state = wait_blink;

		case wait_blink:
			if (arrow_pos)
				state = BLINK_ON;
			break;
        
		case BLINK_ON:
			state = (!arrow_pos || reset) ? init_blink : BLINK_OFF;;
			break;

		case BLINK_OFF:
			state = (!arrow_pos || reset) ? init_blink : BLINK_ON;
			break;
    
        default:
			state = init_blink;
			break;
    }
    switch (state){
        case init_blink:
			arrow_pos = 0;
			break;

        case wait_blink:
			break;
        
        case BLINK_ON:
			LCD_Cursor(arrow_pos);
			LCD_WriteData(arrow);
			LCD_Cursor(0);
			break;
        
		case BLINK_OFF:
			LCD_Cursor(arrow_pos);
			LCD_WriteData(sp);
			LCD_Cursor(0);
			break;

        default:
			break;
    }
    return state;
}


// Handle reset button
int ResetTick(int state) {
	unsigned char reset_button = ~PINA & 0x80;
	switch (state){		
		case ASSIGN_reset:
			state = init_reset;
			break;
		
		case init_reset:
			if (!reset_button) {
				state = wait_reset;
				reset = 0;
			}
			break;
		
		case wait_reset:
			state = (reset_button) ? ASSIGN_reset : wait_reset;
			break;
		
		default:
			state = ASSIGN_reset;
			break;
	}
	switch (state){
		case ASSIGN_reset:
			reset = 1;
			in_menu = 1;
			play_again = 0;
			break;
		
		case init_reset:
			break;
		
		case wait_reset:
			break;
		
		default:
			break;
	}
	return state;
}


// Handle the scroll for Hard or VHard modes
int ScrollTick(int state){
	unsigned char pause_button = ~PINA & 0x40;
	switch (state)	{
		case init_scroll:
			if (!in_menu)
				state = wait_scroll;
				break;

		case wait_scroll:				// Don't scroll in easy
			if (game_mode != easy) {
				scroll_speed = (game_mode == med) ? 2 : 1;
				state = SCROLL;		
				scroll_count = 0;
			}
			break;

		case SCROLL:
			if (in_menu)
				state = init_scroll;				
			break;

		default:
			state = init_scroll;
			break;
	}
	
	switch (state) {
		case init_scroll:
			break;

		case wait_scroll:
			break;

		case SCROLL:
			if (!(paused || pause_button)) {
				++scroll_count;
				if (scroll_count == scroll_speed) {
					moved_player = 1;
					MovePlayerRight();
					scroll_count = 0;
				}
				
			}
			break;

		default:
			break;
	}

	return state;
} 


// Handle win and continue screens
 int VictoryTick(int state) {
	unsigned char start_button = ~PINA & 0x40;
	unsigned char direction = ~PINA & 0x24;
	switch (state){		
		case init_victory:
			state = wait_victory;
			break;
		
		case wait_victory:
			if (winner) {
				state = VICTORY;
				DisplayWinnerMsg();
				arrow_pos = yes_cursor_pos;
				play_again = 1;
			}
			break;
		
		case VICTORY:
			if (start_button) {
				if (play_again) 
					state = init_victory;
				else {
					state = VICTORYx;
					arrow_pos = 0;
					LCD_DisplayString(3, gameover_msg);
					LCD_Cursor(0);
				}								
			}
			break;

		case VICTORYx:
			if (!start_button)
				state = GAME_OVER;
			break;

		case GAME_OVER:
			if (start_button)
				state = init_victory;
			break;

		default:
			state = init_victory;
			break;
	}
	switch (state){
		case init_victory:
			in_menu = 1;
			winner = 0;
			break;
		
		case wait_victory:
			break;
		
		case VICTORY:
			 if (direction == left) {			// Hover over 'Yes'
    			arrow_pos = yes_cursor_pos;
				LCD_Cursor(no_cursor_pos);
    			LCD_WriteData(sp);
    			LCD_Cursor(0);
				play_again = 1;
			 }
			 else if (direction == right) {		// Hover over 'No' 
    			arrow_pos = no_cursor_pos;
				LCD_Cursor(yes_cursor_pos);
    			LCD_WriteData(sp);
    			LCD_Cursor(0);
    			play_again = 0;
			 }
			break;
		
		case GAME_OVER:
			if (start_button) 
				state = init_victory;
			break;

		default:
			break;
	}
	return state;
}


// Handle score in game play and read high score from EEPROM
int ScoreTick(int state) {
	unsigned char pause_button = ~PINA & 0x40;
	switch (state)  {
		case ASSIGN_score:
			state = init_score;
			break;

		case init_score:
			state = wait_score;
			break;
	
		case wait_score:
			if ( !(defeated || in_menu) )
				state = PLAYING;
			break;

		case PLAYING:
			if ( defeated || in_menu )
				state = init_score;
			break;

		default:
			state = ASSIGN_score;
			break;
	}
	switch (state)  {
    	case ASSIGN_score:
    		read_score = eeprom_read_byte(0);
    		if (!read_score) {			// if high_score == 0, set to 60
        		eeprom_write_byte(0, MAX_SCORE);
    		}
    		break;

    	case init_score:
			score = 0;
    		break;
    	
    	case wait_score:
    		break;

    	case PLAYING:
			if ( !(paused || pause_button) )
    			++score;
    		break;

    	default:
    		break;
	}
	return state;

}


// Handle shooter movements and initial shots
// Allows one bullet at a time (fairly fast bullet, so it's reasonable)
int ShooterTick(int state) {
	unsigned char direction = ~PINC & 0x03;
	unsigned char shoot_button = ~PINC & 0x04;
	switch (state) {
		case ASSIGN_shooter:
			state = init_shooter;
			break;

		case init_shooter:
			if (!in_menu)
				state = wait_shooter;
			break;

		case wait_shooter:
			if (in_menu || defeated)
				state = ASSIGN_shooter;
			else if (!paused) {
    			if (shoot_button)
					state = SHOOT;
				else if (direction == shooter_up)
    				state = MOVE_UP_s;
				else 
					state = (direction == shooter_down) ? MOVE_DW_s : wait_shooter;
			}
			break;

		case MOVE_UP_s:
			state = wait_shooter;
			break;

		case MOVE_DW_s:
			state = wait_shooter;
			break;

		case SHOOT:
			state = SHOOTx;
			break;

		case SHOOTx:
			state = (shoot_button) ? SHOOTx : wait_shooter; 
			break;

		default:
			state = ASSIGN_shooter;
			break;
	}
	switch (state) {
		case ASSIGN_shooter:
			shoot_enabled = 1;
			shooter_pos = 0;
			moved_shooter = 1;
			shot_xpos = -1;
			shot_ypos = 0;
			break;

		case init_shooter:
			break;

		case wait_shooter:
			if ( !(defeated || winner) && moved_shooter ){
    			DisplayShooter();
			}
			break;

		case MOVE_UP_s:		// If (on_bot), move top
			if (shooter_pos) {
				ClearShooter();
				shooter_pos = 0;
				moved_shooter = 1;
			}
			break;

		case MOVE_DW_s:		// if (on_top), move bot
			if (!shooter_pos) {		
    			ClearShooter();
    			shooter_pos = 1;
    			moved_shooter = 1;
			}
			break;

		case SHOOT:
			if (shoot_enabled) {
				ShootBullet();
			}
			break;

		case SHOOTx:
			break;

		default:
			break;
	}
	return state;
}


// Move shots and handle player/map/enemy/wall interactions
// Will display custom characters for enemy/wall (will 'pass' through)
int TrackShotsTick(int state) {
	switch (state) {
		case wait_track:
			if (!(in_menu || defeated))
				state = MONITOR;
			break;

		case MONITOR:
			if (in_menu || defeated)
				state = wait_track;
			break;

		default:
			state = wait_track;
			break;
	}
	switch (state) {
		case wait_track:
			break;

		case MONITOR:
			if ( player_shot_x && player_shot_y ) {	// Dead
    			paused = 1;
    			defeated = 1;
    			LCD_DisplayString(1, loser_msg);
			}
			else
				MoveAndCheckBullets();
			break;

		default:
			break;
	}
	return state;
}


// Display top and bot half of map
void DisplayMap() {

	// Can easily be done in for loop
	// Display Top half (if statements draws player for cleaner scrolls)
	LCD_WriteData(map_arr[0][map_pos]);
	LCD_WriteData(map_arr[0][map_pos+1]);
	LCD_WriteData(map_arr[0][map_pos+2]);
	LCD_WriteData(map_arr[0][map_pos+3]);	
	LCD_WriteData(map_arr[0][map_pos+4]);
	LCD_WriteData(map_arr[0][map_pos+5]);
	LCD_WriteData(map_arr[0][map_pos+6]);
	LCD_WriteData(map_arr[0][map_pos+7]);
	LCD_WriteData(map_arr[0][map_pos+8]);
	LCD_WriteData(map_arr[0][map_pos+9]);
	LCD_WriteData(map_arr[0][map_pos+10]);
	LCD_WriteData(map_arr[0][map_pos+11]);
	LCD_WriteData(map_arr[0][map_pos+12]);
	LCD_WriteData(map_arr[0][map_pos+13]);
	LCD_WriteData(map_arr[0][map_pos+14]);
	
	// Send Cursor to Bot half (WriteData func doesn't move rows for you)
	LCD_Cursor(screen_size+1);
	
	//Display Bot half	
	LCD_WriteData(map_arr[1][map_pos]);
	LCD_WriteData(map_arr[1][map_pos+1]);
	LCD_WriteData(map_arr[1][map_pos+2]);
	LCD_WriteData(map_arr[1][map_pos+3]);
	LCD_WriteData(map_arr[1][map_pos+4]);
	LCD_WriteData(map_arr[1][map_pos+5]);
	LCD_WriteData(map_arr[1][map_pos+6]);
	LCD_WriteData(map_arr[1][map_pos+7]);
	LCD_WriteData(map_arr[1][map_pos+8]);
	LCD_WriteData(map_arr[1][map_pos+9]);
	LCD_WriteData(map_arr[1][map_pos+10]);
	LCD_WriteData(map_arr[1][map_pos+11]);
	LCD_WriteData(map_arr[1][map_pos+12]);
	LCD_WriteData(map_arr[1][map_pos+13]);
	LCD_WriteData(map_arr[1][map_pos+14]);
}


// Check for wall and move appropriately (scroll_map or move_player)
void MovePlayerLeft() {
	if (checkL != wall) {		// If wall, don't do anything
    	if (map_pos > begin_of_map) {
        	if (player_xpos == 4) {		// Scroll left
            	--map_pos;
            	moved_map = 1;
        	}
        	else {						// Move left (at end)
				ClearPlayer();
            	--player_xpos;
            	moved_player = 1;
        	}
    	}
    	else if (player_xpos > 1) {		// Move left (at beginning)
        	ClearPlayer();
			--player_xpos;
        	moved_player = 1;
    	}
	}
}


// Check for wall and move appropriately (scroll_map or move_player)
void MovePlayerRight() {
	if (checkR != wall) {
    	if (map_pos < len_map - screen_size) {
        	if (player_xpos == 4) {
            	++map_pos;
            	moved_map = 1;
        	}
        	else {
				ClearPlayer();
            	++player_xpos;
            	moved_player = 1;
        	}
    	}
    	else if (player_xpos < screen_size) {
			ClearPlayer();
        	++player_xpos;
        	moved_player = 1;
    	}
	}
}


// Display PressStart Opening (in menu)
void DisplayStartMsg() {
	LCD_DisplayString(2, title_msg);
	LCD_OverwriteString(20, press_start_msg);
	LCD_Cursor(0);
}


// Display arrow according to current game_mode (in menu)
void UpdateModeArrow() {
	arrow_pos = modes_pos[game_mode] - 1;
	LCD_Cursor(easy_cursor_pos - 1);
	LCD_WriteData(sp);
	LCD_Cursor(med_cursor_pos - 1);
	LCD_WriteData(sp);
	LCD_Cursor(hard_cursor_pos - 1);
	LCD_WriteData(sp);
	LCD_Cursor(0);
}


// Display blank over player_pos, going to display new player_pos 
// Instead of redisplaying the entire map (instead of accessing 2D array 16*)
void ClearPlayer() {
	LCD_Cursor(player_xpos + (screen_size*player_ypos));
	LCD_WriteData(sp);
	LCD_Cursor(0);
}


// Erase shooter
void ClearShooter() {
	LCD_Cursor(shooter_start + (screen_size*shooter_pos));
	LCD_WriteData(sp);
	LCD_Cursor(0);
}


// Display Winner Message and assign new high score (which will write to EEPROM)
// new high score if necessary
void DisplayWinnerMsg(){
	unsigned char best_score = (score < read_score) ? score : read_score;
	if (score < read_score) {
		eeprom_write_byte(0, score);
		read_score = score;
	}
	LCD_DisplayString(1, winner_msg);
	
	// Display score tens digit then ones digit
	LCD_Cursor(score_cursor_pos);
	LCD_WriteData( '0' + ((score / 10) % 10) );
	LCD_WriteData( '0' + (score % 10) );
	
	// Display best tens digit then ones digit
	LCD_Cursor(best_cursor_pos);
	LCD_WriteData( '0' + ((best_score / 10) % 10) );
	LCD_WriteData( '0' + (best_score % 10) );
	
	// Start arrow blink at yes_cursor_pos
	LCD_Cursor(yes_cursor_pos);
	LCD_WriteData(arrow);
	LCD_Cursor(0);
}


// Display Player 
void DisplayPlayer() {
	LCD_Cursor(player_xpos + (screen_size*player_ypos));
	LCD_WriteData(custom_player);
	moved_player = 0;
	LCD_Cursor(0);
}


// Display Shooter
void DisplayShooter() {
	LCD_Cursor(shooter_start + (screen_size*shooter_pos));
	LCD_WriteData(custom_shooter);
	moved_shooter = 0;
	LCD_Cursor(0);
}


// Shoot next possible bullet (disable firing)						
void ShootBullet() {
	LCD_Cursor(shooter_start + (screen_size*shooter_pos) - 1);
	shot_xpos = shooter_start-1;
	shot_ypos = (shooter_pos); //if top_pos, top				
	LCD_WriteData(bullet);
	LCD_Cursor(0);	
	
	shoot_enabled = 0;	// Wait until bullet is done propagating
}

// Does all of the work in TrackShotTick (handles shot interactions)
// And enables firing if shot is done propagating 
void MoveAndCheckBullets() {	
	unsigned char shot_curr_in_map = shot_curr;
	unsigned char shot_curr_in_display = shot_xpos + (screen_size*shot_ypos);
	unsigned char next_map_piece = map_arr[shot_ypos][map_pos + shot_xpos];

	// Shot not done propagating, display next pos, restore prev pos, --xpos 
	if (shot_xpos > 0) {					// If on screen
		
		if (shot_curr_in_map == enemy) {		// Draw custom enemy
			LCD_Cursor(shot_curr_in_display);
			LCD_WriteData(custom_shot_enemy);	
		}
		else if (shot_curr_in_map == wall) {	// Or draw custom wall
			LCD_Cursor(shot_curr_in_display);
			LCD_WriteData(custom_wall);
		}
		else  {									// Or draw '-'
			LCD_Cursor(shot_curr_in_display);
			LCD_WriteData(bullet);
		}
		--shot_xpos;

		// Restore previous column (all except shooter_col and 1st_col)
		if (shot_xpos < shooter_start-2) {
   			LCD_WriteData(next_map_piece);		
		} 
		LCD_Cursor(0);
	}
	
	// Shot finished propagating. Restore 1st_col and make shot available
	else if (shot_xpos == 0) {
		LCD_Cursor(1 + (shot_ypos*screen_size)); 
		LCD_WriteData(next_map_piece);
		LCD_Cursor(0);
		--shot_xpos;		
		shoot_enabled = 1;
	}	
}
