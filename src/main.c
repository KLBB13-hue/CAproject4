#include <stdint.h>

#include "assembly/example.h"
#include "include/lcd/lcd.h"
#include "include/utils.h"

#define MENU_ITEMS 3

#define CELL_SIZE 4
#define FIELD_Y 16
#define GRID_W (LCD_W / CELL_SIZE)
#define GRID_H ((LCD_H - FIELD_Y) / CELL_SIZE)
#define SNAKE_MAX 200

#define NORMAL_STEP_MS 140
#define FAST_STEP_MS 55
#define INPUT_SLICE_MS 10

enum {
  DIR_UP = 0,
  DIR_DOWN,
  DIR_LEFT,
  DIR_RIGHT
};

static int button_last[7];

static uint8_t snake_x[SNAKE_MAX];
static uint8_t snake_y[SNAKE_MAX];
static int snake_len;
static int snake_dir;
static int pending_dir;
static uint8_t food_x;
static uint8_t food_y;
static uint16_t score;
static uint32_t rng_state = 0x1234abcd;

void Inp_init(void) {
  rcu_periph_clock_enable(RCU_GPIOA);
  rcu_periph_clock_enable(RCU_GPIOC);

  gpio_init(GPIOA, GPIO_MODE_IPD, GPIO_OSPEED_50MHZ,
            GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);
  gpio_init(GPIOC, GPIO_MODE_IPD, GPIO_OSPEED_50MHZ,
            GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);
}

void IO_init(void) {
  Inp_init();
  Lcd_Init();
}

static int any_button_down(void) {
  int i;
  for (i = 0; i < 7; i++) {
    if (Get_Button(i)) {
      return 1;
    }
  }
  return 0;
}

static void reset_button_edges(void) {
  int i;
  for (i = 0; i < 7; i++) {
    button_last[i] = 0;
  }
}

static void wait_buttons_released(void) {
  while (any_button_down()) {
    delay_1ms(20);
  }
  reset_button_edges();
}

static int button_pressed(int ch) {
  int now = Get_Button(ch);
  int pressed = now && !button_last[ch];
  button_last[ch] = now;
  return pressed;
}

static uint32_t next_rand(void) {
  rng_state = rng_state * 1664525u + 1013904223u;
  return rng_state;
}

static int snake_has_cell(uint8_t x, uint8_t y, int limit) {
  int i;
  for (i = 0; i < limit; i++) {
    if (snake_x[i] == x && snake_y[i] == y) {
      return 1;
    }
  }
  return 0;
}

static void spawn_food(void) {
  int tries;
  int x;
  int y;

  for (tries = 0; tries < 256; tries++) {
    x = (int)(next_rand() % GRID_W);
    y = (int)(next_rand() % GRID_H);
    if (!snake_has_cell((uint8_t)x, (uint8_t)y, snake_len)) {
      food_x = (uint8_t)x;
      food_y = (uint8_t)y;
      return;
    }
  }

  for (y = 0; y < GRID_H; y++) {
    for (x = 0; x < GRID_W; x++) {
      if (!snake_has_cell((uint8_t)x, (uint8_t)y, snake_len)) {
        food_x = (uint8_t)x;
        food_y = (uint8_t)y;
        return;
      }
    }
  }
}

static void snake_init(void) {
  int i;
  snake_len = 4;
  snake_dir = DIR_RIGHT;
  pending_dir = DIR_RIGHT;
  score = 0;

  for (i = 0; i < snake_len; i++) {
    snake_x[i] = (uint8_t)(GRID_W / 2 - i);
    snake_y[i] = (uint8_t)(GRID_H / 2);
  }

  rng_state ^= (uint32_t)(rank_get_count() + 1u) * 97u;
  spawn_food();
}

static void set_pending_dir(int new_dir) {
  if ((snake_dir == DIR_UP && new_dir == DIR_DOWN) ||
      (snake_dir == DIR_DOWN && new_dir == DIR_UP) ||
      (snake_dir == DIR_LEFT && new_dir == DIR_RIGHT) ||
      (snake_dir == DIR_RIGHT && new_dir == DIR_LEFT)) {
    return;
  }
  pending_dir = new_dir;
}

static void read_game_input(void) {
  if (Get_Button(JOY_UP)) {
    set_pending_dir(DIR_UP);
  } else if (Get_Button(JOY_DOWN)) {
    set_pending_dir(DIR_DOWN);
  } else if (Get_Button(JOY_LEFT)) {
    set_pending_dir(DIR_LEFT);
  } else if (Get_Button(JOY_RIGHT)) {
    set_pending_dir(DIR_RIGHT);
  }
}

static int snake_step(void) {
  int nx = snake_x[0];
  int ny = snake_y[0];
  int eat;
  int check_len;
  int i;

  snake_dir = pending_dir;
  if (snake_dir == DIR_UP) {
    ny--;
  } else if (snake_dir == DIR_DOWN) {
    ny++;
  } else if (snake_dir == DIR_LEFT) {
    nx--;
  } else {
    nx++;
  }

  if (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H) {
    return 0;
  }

  eat = ((uint8_t)nx == food_x && (uint8_t)ny == food_y);
  check_len = eat ? snake_len : snake_len - 1;
  if (snake_has_cell((uint8_t)nx, (uint8_t)ny, check_len)) {
    return 0;
  }

  if (eat && snake_len < SNAKE_MAX) {
    snake_len++;
  }

  for (i = snake_len - 1; i > 0; i--) {
    snake_x[i] = snake_x[i - 1];
    snake_y[i] = snake_y[i - 1];
  }

  snake_x[0] = (uint8_t)nx;
  snake_y[0] = (uint8_t)ny;

  if (eat) {
    score += 10;
    spawn_food();
  }

  return 1;
}

static void draw_cell(uint8_t gx, uint8_t gy, u16 color) {
  u16 x = (u16)(gx * CELL_SIZE);
  u16 y = (u16)(FIELD_Y + gy * CELL_SIZE);
  LCD_Fill(x, y, x + CELL_SIZE - 1, y + CELL_SIZE - 1, color);
}

static void draw_game(void) {
  int i;

  LCD_Clear(BLACK);
  LCD_ShowString(2, 0, (const u8 *)"S:", WHITE);
  LCD_ShowNum(20, 0, score, 4, YELLOW);
  if (Get_Button(BUTTON_2)) {
    LCD_ShowString(102, 0, (const u8 *)"FAST", RED);
  }
  LCD_DrawLine(0, FIELD_Y - 1, LCD_W - 1, FIELD_Y - 1, GRAY);

  draw_cell(food_x, food_y, YELLOW);
  for (i = snake_len - 1; i >= 0; i--) {
    draw_cell(snake_x[i], snake_y[i], i == 0 ? GREEN : LIGHTGREEN);
  }
}

static void draw_menu(int selected) {
  static const u8 *items[MENU_ITEMS] = {
      (const u8 *)"1-1 START",
      (const u8 *)"1-2 RANK",
      (const u8 *)"1-3 INFO",
  };
  int i;

  LCD_Clear(BLACK);
  LCD_ShowString(56, 2, (const u8 *)"SNAKE", GREEN);
  for (i = 0; i < MENU_ITEMS; i++) {
    u16 y = (u16)(22 + i * 16);
    if (i == selected) {
      LCD_ShowString(24, y, (const u8 *)">", YELLOW);
      LCD_ShowString(40, y, items[i], YELLOW);
    } else {
      LCD_ShowString(40, y, items[i], WHITE);
    }
  }
}

static int run_menu(void) {
  int selected = 0;

  wait_buttons_released();
  draw_menu(selected);
  while (1) {
    int up = button_pressed(JOY_UP) | button_pressed(JOY_LEFT);
    int down = button_pressed(JOY_DOWN) | button_pressed(JOY_RIGHT);

    if (up) {
      selected = (selected + MENU_ITEMS - 1) % MENU_ITEMS;
      draw_menu(selected);
    }
    if (down) {
      selected = (selected + 1) % MENU_ITEMS;
      draw_menu(selected);
    }
    if (button_pressed(JOY_CTR)) {
      return selected;
    }
    delay_1ms(30);
  }
}

static void draw_rank(void) {
  int i;

  LCD_Clear(BLACK);
  for (i = 0; i < 5; i++) {
    u16 y = (u16)(i * 16);
    LCD_ShowNum(8, y, (u16)(i + 1), 1, WHITE);
    LCD_ShowString(18, y, (const u8 *)".", WHITE);
    LCD_ShowNum(36, y, (u16)rank_get_score(i), 4, GREEN);
  }
}

static void run_rank(void) {
  wait_buttons_released();
  draw_rank();
  while (1) {
    if (button_pressed(BUTTON_1)) {
      return;
    }
    delay_1ms(30);
  }
}

static void run_info(void) {
  wait_buttons_released();
  LCD_Clear(BLACK);
  LCD_ShowString(20, 4, (const u8 *)"1-3 INFO", YELLOW);
  LCD_ShowString(8, 24, (const u8 *)"JOY MOVE", WHITE);
  LCD_ShowString(8, 40, (const u8 *)"SW2 FAST", WHITE);
  LCD_ShowString(8, 56, (const u8 *)"SW1 BACK", WHITE);
  while (1) {
    if (button_pressed(BUTTON_1)) {
      return;
    }
    delay_1ms(30);
  }
}

static int run_game_over(void) {
  wait_buttons_released();
  LCD_Clear(BLACK);
  LCD_ShowString(36, 4, (const u8 *)"GAME OVER", RED);
  LCD_ShowString(32, 24, (const u8 *)"SCORE", WHITE);
  LCD_ShowNum(84, 24, score, 4, YELLOW);
  LCD_ShowString(24, 44, (const u8 *)"CTR AGAIN", GREEN);
  LCD_ShowString(28, 60, (const u8 *)"SW1 MENU", WHITE);

  while (1) {
    if (button_pressed(JOY_CTR)) {
      return 1;
    }
    if (button_pressed(BUTTON_1)) {
      return 0;
    }
    delay_1ms(30);
  }
}

static int wait_next_step_or_back(void) {
  int elapsed = 0;
  int step_ms = Get_Button(BUTTON_2) ? FAST_STEP_MS : NORMAL_STEP_MS;

  while (elapsed < step_ms) {
    read_game_input();
    if (button_pressed(BUTTON_1)) {
      return 1;
    }
    delay_1ms(INPUT_SLICE_MS);
    elapsed += INPUT_SLICE_MS;
  }

  return 0;
}

static int run_game(void) {
  snake_init();
  wait_buttons_released();
  draw_game();

  while (1) {
    if (wait_next_step_or_back()) {
      return 0;
    }
    if (!snake_step()) {
      rank_add_score(score);
      return run_game_over();
    }
    draw_game();
  }
}

int main(void) {
  IO_init();
  LCD_Clear(BLACK);

  while (1) {
    int selected = run_menu();
    if (selected == 0) {
      while (run_game()) {
      }
    } else if (selected == 1) {
      run_rank();
    } else {
      run_info();
    }
  }
}
