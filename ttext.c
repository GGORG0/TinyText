#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/types.h>

/* DEFINES */

#define CTRL_KEY(k) ((k)&0x1f)
#define TTEXT_VERSION "0.1"
#define TAB_SIZE 2

enum editorKeymap
{
  ARROW_UP = 1000,
  ARROW_DOWN,
  ARROW_LEFT,
  ARROW_RIGHT,
  PAGE_UP,
  PAGE_DOWN,
  HOME_KEY,
  END_KEY,
  DEL_KEY
};

/* DATA */

typedef struct editorRow
{
  int size;
  int render_size;
  char *chars;
  char *render_chars;
} editorRow;

struct editorConfig
{
  int cursor_x;
  int cursor_render_x;
  int cursor_y;

  int screen_rows;
  int screen_cols;

  int row_count;
  editorRow *row;

  int row_offset;
  int col_offset;

  struct termios orig_termios;
};
struct editorConfig E;

/* TERMINAL */

void tOnError(const char *msg)
{
  write(STDOUT_FILENO, "\x1b[2J", 4); // clear screen
  write(STDOUT_FILENO, "\x1b[H", 3);  // move cursor to top left

  perror(msg);
  exit(1);
}

void tDisableRawMode()
{
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    tOnError("tcsetattr");
}

void tEnableRawMode()
{
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
    tOnError("tcgetattr");

  atexit(tDisableRawMode);

  struct termios raw = E.orig_termios;

  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    tOnError("tcsetattr");
}

int tReadKeypress()
{
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
  {
    if (nread == -1 && errno != EAGAIN)
      tOnError("read");
  }

  if (c == '\x1b')
  {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1)
      return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1)
      return '\x1b';
    if (seq[0] == '[')
    {
      if (seq[1] >= '0' && seq[1] <= '9')
      {
        if (read(STDIN_FILENO, &seq[2], 1) != 1)
          return '\x1b';
        if (seq[2] == '~')
        {
          switch (seq[1])
          {
          case '1':
            return HOME_KEY;
          case '3':
            return DEL_KEY;
          case '4':
            return END_KEY;
          case '5':
            return PAGE_UP;
          case '6':
            return PAGE_DOWN;
          case '7':
            return HOME_KEY;
          case '8':
            return END_KEY;
          }
        }
      }
      else
      {
        switch (seq[1])
        {
        case 'A':
          return ARROW_UP;
        case 'B':
          return ARROW_DOWN;
        case 'C':
          return ARROW_RIGHT;
        case 'D':
          return ARROW_LEFT;
        case 'H':
          return HOME_KEY;
        case 'F':
          return END_KEY;
        }
      }
    }
    else if (seq[0] == 'O')
    {
      switch (seq[1])
      {
      case 'H':
        return HOME_KEY;
      case 'F':
        return END_KEY;
      }
    }

    return '\x1b';
  }
  else
  {
    return c;
  }
}

int tGetCursorPosition(int *rows, int *cols)
{
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
    return -1;

  while (i < sizeof(buf) - 1)
  {
    if (read(STDIN_FILENO, &buf[i], 1) != 1)
      break;
    if (buf[i] == 'R')
      break;
    i++;
  }

  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[')
    return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
    return -1;
  return 0;
}

int tGetTerminalSize(int *rows, int *cols)
{
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0 || ws.ws_row == 0)
  {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return -1;
    return tGetCursorPosition(rows, cols);
  }
  else
  {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/* APPEND BUFFER */

struct abuf
{
  char *b;
  int len;
};
#define ABUF_INIT \
  {               \
    NULL, 0       \
  }

void abAppend(struct abuf *ab, const char *s, int len)
{
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL)
    return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab)
{
  free(ab->b);
}

/* EDITOR */

void eDrawRows(struct abuf *ab)
{
  int y;
  for (y = 0; y < E.screen_rows; y++)
  {
    int filerow = y + E.row_offset;
    if (filerow >= E.row_count)
    {
      if (E.row_count == 0 && y == E.screen_rows / 2)
      {
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
                                  "Welcome to TinyText v%s!", TTEXT_VERSION);

        if (welcomelen > E.screen_cols)
          welcomelen = E.screen_cols;

        int padding = (E.screen_cols - welcomelen) / 2;
        if (padding)
        {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--)
          abAppend(ab, " ", 1);

        abAppend(ab, welcome, welcomelen);
      }
      else if (E.row_count == 0 && y == E.screen_rows / 2 + 1 && E.screen_cols >= 50)
      {
        char *subtitle = "You can provide a file in the command-line args";
        int subtitlelen = strlen(subtitle);

        if (subtitlelen > E.screen_cols)
          subtitlelen = E.screen_cols;

        int padding = (E.screen_cols - subtitlelen) / 2;
        if (padding)
        {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--)
          abAppend(ab, " ", 1);

        abAppend(ab, subtitle, subtitlelen);
      }
      else
      {
        abAppend(ab, "~", 1);
      }
    }
    else
    {
      int length = E.row[filerow].render_size - E.col_offset;
      if (length < 0)
      {
        length = 0;
        abAppend(ab, "<", 1);
      }
      else
      {
        if (length > E.screen_cols)
          length = E.screen_cols;
        abAppend(ab, &E.row[filerow].render_chars[E.col_offset], length);
      }
    }

    abAppend(ab, "\x1b[K", 3); // clear to the end of the line
    if (y < E.screen_rows - 1)
    {
      abAppend(ab, "\r\n", 2);
    }
  }
}

void eUpdateRow(editorRow *row)
{
  int tab_count = 0;

  for (int j = 0; j < row->size; j++)
    if (row->chars[j] == '\t')
      tab_count++;
  free(row->render_chars);
  row->render_chars = malloc(row->size + tab_count * (TAB_SIZE - 1) + 1);

  int idx = 0;
  for (int j = 0; j < row->size; j++)
  {
    if (row->chars[j] == '\t')
    {
      row->render_chars[idx++] = ' ';
      while (idx % TAB_SIZE != 0)
        row->render_chars[idx++] = ' ';
    }
    else
    {
      row->render_chars[idx++] = row->chars[j];
    }
  }

  row->render_chars[idx] = '\0';
  row->render_size = idx;
}

void eAppendRow(char *s, size_t len)
{
  E.row = realloc(E.row, sizeof(editorRow) * (E.row_count + 1));

  int at = E.row_count;
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';

  E.row[at].render_size = 0;
  E.row[at].render_chars = NULL;
  eUpdateRow(&E.row[at]);

  E.row_count++;
}

void eClearScreen(struct abuf *ab)
{
  abAppend(ab, "\x1b[2J", 4);
}

void eResetCursor(struct abuf *ab)
{
  abAppend(ab, "\x1b[H", 3);
}

void eSetCursorPos(struct abuf *ab, int x, int y)
{
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", y, x);
  abAppend(ab, buf, strlen(buf));
}

void eSetCursorVisibility(struct abuf *ab, int visible)
{
  if (visible)
    abAppend(ab, "\x1b[?25h", 6);
  else
    abAppend(ab, "\x1b[?25l", 6);
}

int eRowCursorXToRenderX(editorRow *row, int cx)
{
  int rx = 0;
  int j;
  for (j = 0; j < cx; j++)
  {
    if (row->chars[j] == '\t')
      rx += (TAB_SIZE - 1) - (rx % TAB_SIZE);
    rx++;
  }
  return rx;
}

void eUpdateScrollOffsets()
{
  E.cursor_render_x = 0;
  if (E.cursor_y < E.row_count)
  {
    E.cursor_render_x = eRowCursorXToRenderX(&E.row[E.cursor_y], E.cursor_x);
  }

  // vertical scrolling
  if (E.cursor_y < E.row_offset)
  {
    E.row_offset = E.cursor_y;
  }
  if (E.cursor_y >= E.row_offset + E.screen_rows)
  {
    E.row_offset = E.cursor_y - E.screen_rows + 1;
  }

  // horizontal scrolling
  if (E.cursor_render_x < E.col_offset)
  {
    E.col_offset = E.cursor_render_x;
  }
  if (E.cursor_render_x >= E.col_offset + E.screen_cols)
  {
    E.col_offset = E.cursor_render_x - E.screen_cols + 1;
  }
}

void eRefreshScreen()
{
  eUpdateScrollOffsets();

  struct abuf ab = ABUF_INIT;

  eSetCursorVisibility(&ab, 0);
  eResetCursor(&ab);

  eDrawRows(&ab);

  eSetCursorPos(&ab, E.cursor_render_x - E.col_offset + 1, E.cursor_y - E.row_offset + 1);
  eSetCursorVisibility(&ab, 1);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

void eWaitAndProcessKey()
{
  int c = tReadKeypress();
  editorRow *currentRow = (E.cursor_y >= E.row_count) ? NULL : &E.row[E.cursor_y];
  switch (c)
  {
  case CTRL_KEY('c'):
  case CTRL_KEY('q'):
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    printf("Bye!\r\n");
    exit(0);
    break;

  case ARROW_LEFT:
    if (E.cursor_x > 0)
      E.cursor_x--;
    else if (E.cursor_x == 0 && E.cursor_y > 0)
    {
      E.cursor_y--;
      E.cursor_x = E.row[E.cursor_y].size;
    }
    break;
  case ARROW_RIGHT:
    if (currentRow && E.cursor_x < currentRow->size)
      E.cursor_x++;
    else if (currentRow && E.cursor_x == currentRow->size)
    {
      E.cursor_y++;
      E.cursor_x = 0;
    }
    break;
  case ARROW_UP:
    if (E.cursor_y > 0)
      E.cursor_y--;
    break;
  case ARROW_DOWN:
    if (E.cursor_y < E.row_count)
      E.cursor_y++;
    else if (E.row_offset < E.row_count - 1)
      E.row_offset++;
    break;

  case PAGE_UP:
    if (E.cursor_y >= 10)
      E.cursor_y -= 10;
    else
      E.cursor_y = 0;
    break;
  case PAGE_DOWN:
    if (E.cursor_y <= E.row_count - 10)
      E.cursor_y += 10;
    else
      E.cursor_y = E.row_count;
    break;

  case HOME_KEY:
    E.cursor_x = 0;
    break;
  case END_KEY:
    if (E.cursor_y < E.row_count)
      E.cursor_x = E.row[E.cursor_y].size;
    break;
  }

  currentRow = (E.cursor_y >= E.row_count) ? NULL : &E.row[E.cursor_y];
  if (currentRow && E.cursor_x > currentRow->size)
    E.cursor_x = currentRow->size;
  else if (!currentRow)
    E.cursor_x = 0;
}

void eInitEditor()
{
  // init editor config
  E.cursor_x = 0;
  E.cursor_render_x = 0;
  E.cursor_y = 0;
  E.row_count = 0;
  E.row = NULL;
  E.row_offset = 0;
  E.col_offset = 0;

  if (tGetTerminalSize(&E.screen_rows, &E.screen_cols) == -1)
    tOnError("tGetTerminalSize");
}

/* FILE IO */

void fOpen(char *filename)
{
  FILE *fp = fopen(filename, "r");
  if (!fp)
    tOnError("fopen");

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1)
  {
    while (linelen > 0 && (line[linelen - 1] == '\n' ||
                           line[linelen - 1] == '\r'))
      linelen--;

    eAppendRow(line, linelen);
  }
  free(line);
  fclose(fp);
}

/* MAIN FUNCTION */

int main(int argc, char *argv[])
{
  tEnableRawMode();
  eInitEditor();
  if (argc >= 2)
    fOpen(argv[1]);

  while (1)
  {
    eRefreshScreen();
    eWaitAndProcessKey();
  }

  return 0;
}
