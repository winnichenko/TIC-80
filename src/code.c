// MIT License

// Copyright (c) 2017 Vadim Grigoruk @nesbox // grigoruk@gmail.com

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "code.h"
#include "history.h"

#include <ctype.h>

#define TEXT_CURSOR_DELAY (TIC80_FRAMERATE / 2)
#define TEXT_CURSOR_BLINK_PERIOD TIC80_FRAMERATE
#define BOOKMARK_WIDTH 7
#define CODE_EDITOR_WIDTH (TIC80_WIDTH - BOOKMARK_WIDTH)
#define CODE_EDITOR_HEIGHT (TIC80_HEIGHT - TOOLBAR_SIZE - STUDIO_TEXT_HEIGHT)
#define TEXT_BUFFER_HEIGHT (CODE_EDITOR_HEIGHT / STUDIO_TEXT_HEIGHT)

enum
{
    SyntaxTypeString    = offsetof(struct SyntaxColors, string),
    SyntaxTypeNumber    = offsetof(struct SyntaxColors, number),
    SyntaxTypeKeyword   = offsetof(struct SyntaxColors, keyword),
    SyntaxTypeApi       = offsetof(struct SyntaxColors, api),
    SyntaxTypeComment   = offsetof(struct SyntaxColors, comment),
    SyntaxTypeSign      = offsetof(struct SyntaxColors, sign),
    SyntaxTypeVar       = offsetof(struct SyntaxColors, var),
    SyntaxTypeOther     = offsetof(struct SyntaxColors, other),
};

static void history(Code* code)
{
    if(history_add(code->history))
        history_add(code->cursorHistory);
}

static void drawStatus(Code* code)
{
    enum {Height = TIC_FONT_HEIGHT + 1, StatusY = TIC80_HEIGHT - TIC_FONT_HEIGHT};

    tic_api_rect(code->tic, 0, TIC80_HEIGHT - Height, TIC80_WIDTH, Height, tic_color_12);
    tic_api_print(code->tic, code->statusLine, 0, StatusY, getConfig()->theme.code.bg, 1);
    tic_api_print(code->tic, code->statusSize, TIC80_WIDTH - strlen(code->statusSize) * TIC_FONT_WIDTH, 
        StatusY, getConfig()->theme.code.bg, 1);
}

static void drawBookmarks(Code* code)
{
    tic_api_rect(code->tic, 0, TOOLBAR_SIZE, BOOKMARK_WIDTH, TIC80_HEIGHT - TOOLBAR_SIZE, tic_color_14);
}

static inline s32 getFontWidth(Code* code)
{
    //return code->altFont ? TIC_ALTFONT_WIDTH : TIC_FONT_WIDTH;
    return TIC_FONT_WIDTH;
}

static inline void drawChar(tic_mem* tic, char symbol, s32 x, s32 y, u8 color)
{
    tic_api_print(tic, (char[]){symbol, '\0'}, x, y, color, 1);
}

static void drawCursor(Code* code, s32 x, s32 y, char symbol)
{
    bool inverse = code->cursor.delay || code->tickCounter % TEXT_CURSOR_BLINK_PERIOD < TEXT_CURSOR_BLINK_PERIOD / 2;

    if(inverse)
    {
        if(code->shadowText)
            tic_api_rect(code->tic, x, y, getFontWidth(code)+1, TIC_FONT_HEIGHT+1, 0);

        tic_api_rect(code->tic, x-1, y-1, getFontWidth(code)+1, TIC_FONT_HEIGHT+1, getConfig()->theme.code.cursor);

		if (symbol) 
		{
			if (symbol == 9)
			{
				if(code->show_tabs)
					drawChar(code->tic, getConfig()->theme.code.tab_symbol, x, y, getConfig()->theme.code.bg);
			}
			else
			{
				drawChar(code->tic, symbol, x, y, getConfig()->theme.code.bg);
			}
		}
    }
}

static void drawMatchedDelim(Code* code, s32 x, s32 y, char symbol, u8 color)
{
    tic_api_rectb(code->tic, x-1, y-1, (getFontWidth(code))+1, TIC_FONT_HEIGHT+1,
                  getConfig()->theme.code.cursor);
    drawChar(code->tic, symbol, x, y, color);
}

static void drawCode(Code* code, bool withCursor)
{
    tic_rect rect = {BOOKMARK_WIDTH, TOOLBAR_SIZE, CODE_EDITOR_WIDTH, CODE_EDITOR_HEIGHT};

    s32 xStart = rect.x - code->scroll.x * getFontWidth(code);
    s32 x = xStart;
    s32 y = rect.y - code->scroll.y * STUDIO_TEXT_HEIGHT;
    const char* pointer = code->src;
	u8 tab = getConfig()->theme.code.tab_symbol;
	bool showtabs = code->show_tabs;
    u8 selectColor = getConfig()->theme.code.select;
    const struct tic_code_theme* theme = &getConfig()->theme.code.syntax;
    const u8* syntaxPointer = code->syntax;

    struct { char* start; char* end; } selection = 
    {
        MIN(code->cursor.selection, code->cursor.position),
        MAX(code->cursor.selection, code->cursor.position)
    };

    struct { s32 x; s32 y; char symbol; } cursor = {-1, -1, 0};
    struct { s32 x; s32 y; char symbol; u8 color; } matchedDelim = {-1, -1, 0, 0};

    while(*pointer)
    {
        char symbol = *pointer;

        if(x >= -getFontWidth(code) && x < TIC80_WIDTH && y >= -TIC_FONT_HEIGHT && y < TIC80_HEIGHT )
        {
            if(code->cursor.selection && pointer >= selection.start && pointer < selection.end)
            {
                /*
				if(code->shadowText)
                    tic_api_rect(code->tic, x, y, getFontWidth(code)+1, TIC_FONT_HEIGHT+1, tic_color_0);
				*/
                tic_api_rect(code->tic, x-1, y-1, getFontWidth(code)+1, TIC_FONT_HEIGHT+1, selectColor);
				if (symbol == 9)//draw special symbol instead of tab code 009
				{
					if (showtabs)
						drawChar(code->tic, tab, x, y, tic_color_14);
				}
				else
				{
					drawChar(code->tic, symbol, x, y, tic_color_15);
				}
            }
            else 
            {
               /*
				if(code->shadowText)
                    drawChar(code->tic, symbol, x+1, y+1, 0);
				*/
				if (symbol == 9) //draw special symbol instead of tab code 009
				{
					if (showtabs)
						//drawChar(code->tic, tab, x, y, theme->colors[*syntaxPointer]);
						drawChar(code->tic, tab, x, y, tic_color_14);
				}
				else
				{
					drawChar(code->tic, symbol, x, y, theme->colors[*syntaxPointer]);
				}
            }
        }

        if(code->cursor.position == pointer)
            cursor.x = x, cursor.y = y, cursor.symbol = symbol;

        if(code->matchedDelim == pointer)
        {
            matchedDelim.x = x, matchedDelim.y = y, matchedDelim.symbol = symbol,
                matchedDelim.color = theme->colors[*syntaxPointer];
        }

        if(symbol == '\n')
        {
            x = xStart;
            y += STUDIO_TEXT_HEIGHT;
        }
        else x += getFontWidth(code);

        pointer++;
        syntaxPointer++;
    }

    drawBookmarks(code);

    if(code->cursor.position == pointer)
        cursor.x = x, cursor.y = y;

    if(withCursor && cursor.x >= 0 && cursor.y >= 0)
        drawCursor(code, cursor.x, cursor.y, cursor.symbol, tab);

    if(matchedDelim.symbol) {
        drawMatchedDelim(code, matchedDelim.x, matchedDelim.y,
                         matchedDelim.symbol, matchedDelim.color);
    }
}

static void getCursorPosition(Code* code, s32* x, s32* y)
{
    *x = 0;
    *y = 0;

    const char* pointer = code->src;

    while(*pointer)
    {
        if(code->cursor.position == pointer) return;

        if(*pointer == '\n')
        {
            *x = 0;
            (*y)++;
        }
        else (*x)++;

        pointer++;
    }
}

static s32 getLinesCount(Code* code)
{
    char* text = code->src;
    s32 count = 0;

    while(*text)
        if(*text++ == '\n')
            count++;

    return count;
}

static void removeInvalidChars(char* code)
{
    // remove \r symbol
    char* s; char* d;
    for(s = d = code; (*d = *s); d += (*s++ != '\r'));
}

const char* findMatchedDelim(Code* code, const char* current)
{
    const char* start = code->src;
    // delimiters inside comments and strings don't get to be matched!
    if(code->syntax[current - start] == SyntaxTypeComment ||
       code->syntax[current - start] == SyntaxTypeString) return 0;

    char initial = *current;
    char seeking = 0;
    s8 dir = (initial == '(' || initial == '[' || initial == '{') ? 1 : -1;
    switch (initial)
    {
    case '(': seeking = ')'; break;
    case ')': seeking = '('; break;
    case '[': seeking = ']'; break;
    case ']': seeking = '['; break;
    case '{': seeking = '}'; break;
    case '}': seeking = '{'; break;
    default: return NULL;
    }

    while(*current && (start < current))
    {
        current += dir;
        // skip over anything inside a comment or string
        if(code->syntax[current - start] == SyntaxTypeComment ||
           code->syntax[current - start] == SyntaxTypeString) continue;
        if(*current == seeking) return current;
        if(*current == initial) current = findMatchedDelim(code, current);
    }

    return NULL;
}

static void updateEditor(Code* code)
{
    s32 column = 0;
    s32 line = 0;
    getCursorPosition(code, &column, &line);
    if(getConfig()->theme.code.matchDelimiters)
        code->matchedDelim = findMatchedDelim(code, code->cursor.position);

    const s32 BufferWidth = CODE_EDITOR_WIDTH / getFontWidth(code);

    if(column < code->scroll.x) code->scroll.x = column;
    else if(column >= code->scroll.x + BufferWidth)
        code->scroll.x = column - BufferWidth + 1;

    if(line < code->scroll.y) code->scroll.y = line;
    else if(line >= code->scroll.y + TEXT_BUFFER_HEIGHT)
        code->scroll.y = line - TEXT_BUFFER_HEIGHT + 1;

    code->cursor.delay = TEXT_CURSOR_DELAY;

    {
        sprintf(code->statusLine, "line %i/%i col %i", line + 1, getLinesCount(code) + 1, column + 1);
        sprintf(code->statusSize, "%i/%i", (u32)strlen(code->src), TIC_CODE_SIZE);
    }
}

static inline bool islineend(char c) {return c == '\n' || c == '\0';}
static inline bool isalpha_(char c) {return isalpha(c) || c == '_';}
static inline bool isalnum_(char c) {return isalnum(c) || c == '_';}

static void parseCode(const tic_script_config* config, const char* start, u8* syntax)
{
    const char* ptr = start;

    const char* blockCommentStart = NULL;
    const char* blockStringStart = NULL;
    const char* blockStdStringStart = NULL;
    const char* singleCommentStart = NULL;
    const char* wordStart = NULL;
    const char* numberStart = NULL;

    while(true)
    {
        char c = ptr[0];

        if(blockCommentStart)
        {
            const char* end = strstr(ptr, config->blockCommentEnd);

            ptr = end ? end + strlen(config->blockCommentEnd) : blockCommentStart + strlen(blockCommentStart);
            memset(syntax + (blockCommentStart - start), SyntaxTypeComment, ptr - blockCommentStart);
            blockCommentStart = NULL;
            continue;
        }
        else if(blockStringStart)
        {
            const char* end = strstr(ptr, config->blockStringEnd);

            ptr = end ? end + strlen(config->blockStringEnd) : blockStringStart + strlen(blockStringStart);
            memset(syntax + (blockStringStart - start), SyntaxTypeString, ptr - blockStringStart);
            blockStringStart = NULL;
            continue;
        }
        else if(blockStdStringStart)
        {
            const char* blockStart = blockStdStringStart+1;

            while(true)
            {
                const char* pos = strchr(blockStart, *blockStdStringStart);
                
                if(pos)
                {
                    if(*(pos-1) == '\\' && *(pos-2) != '\\') blockStart = pos + 1;
                    else
                    {
                        ptr = pos + 1;
                        break;
                    }
                }
                else
                {
                    ptr = blockStdStringStart + strlen(blockStdStringStart);
                    break;
                }
            }

            memset(syntax + (blockStdStringStart - start), SyntaxTypeString, ptr - blockStdStringStart);
            blockStdStringStart = NULL;
            continue;
        }
        else if(singleCommentStart)
        {
            while(!islineend(*ptr))ptr++;

            memset(syntax + (singleCommentStart - start), SyntaxTypeComment, ptr - singleCommentStart);
            singleCommentStart = NULL;
            continue;
        }
        else if(wordStart)
        {
            while(!islineend(*ptr) && isalnum_(*ptr)) ptr++;

            s32 len = ptr - wordStart;
            bool keyword = false;
            {
                for(s32 i = 0; i < config->keywordsCount; i++)
                    if(len == strlen(config->keywords[i]) && memcmp(wordStart, config->keywords[i], len) == 0)
                    {
                        memset(syntax + (wordStart - start), SyntaxTypeKeyword, len);
                        keyword = true;
                        break;
                    }
            }

            if(!keyword)
            {
                #define API_KEYWORD_DEF(name, ...) #name,
                static const char* const ApiKeywords[] = {TIC_FN, SCN_FN, OVR_FN, TIC_API_LIST(API_KEYWORD_DEF)};
                #undef API_KEYWORD_DEF

                for(s32 i = 0; i < COUNT_OF(ApiKeywords); i++)
                    if(len == strlen(ApiKeywords[i]) && memcmp(wordStart, ApiKeywords[i], len) == 0)
                    {
                        memset(syntax + (wordStart - start), SyntaxTypeApi, len);
                        break;
                    }
            }

            wordStart = NULL;
            continue;
        }
        else if(numberStart)
        {
            while(!islineend(*ptr))
            {
                char c = *ptr;

                if(isdigit(c)) ptr++;
                else if(numberStart[0] == '0' 
                    && (numberStart[1] == 'x' || numberStart[1] == 'X') 
                    && isxdigit(numberStart[2]))
                {
                    if((ptr - numberStart < 2) || isxdigit(c)) ptr++;
                    else break;
                }
                else if(c == '.' || c == 'e' || c == 'E')
                {
                    if(isdigit(ptr[1])) ptr++;
                    else break;
                }
                else break;
            }

            memset(syntax + (numberStart - start), SyntaxTypeNumber, ptr - numberStart);
            numberStart = NULL;
            continue;
        }
        else
        {
            if(config->blockCommentStart && memcmp(ptr, config->blockCommentStart, strlen(config->blockCommentStart)) == 0)
            {
                blockCommentStart = ptr;
                ptr += strlen(config->blockCommentStart);
                continue;
            }
            if(config->blockStringStart && memcmp(ptr, config->blockStringStart, strlen(config->blockStringStart)) == 0)
            {
                blockStringStart = ptr;
                ptr += strlen(config->blockStringStart);
                continue;
            }
            else if(c == '"' || c == '\'')
            {
                blockStdStringStart = ptr;
                ptr++;
                continue;
            }
            else if(config->singleComment && memcmp(ptr, config->singleComment, strlen(config->singleComment)) == 0)
            {
                singleCommentStart = ptr;
                ptr += strlen(config->singleComment);
                continue;
            }
            else if(isalpha_(c))
            {
                wordStart = ptr;
                ptr++;
                continue;
            }
            else if(isdigit(c) || (c == '.' && isdigit(ptr[1])))
            {
                numberStart = ptr;
                ptr++;
                continue;
            }
            else if(ispunct(c)) syntax[ptr - start] = SyntaxTypeSign;
            else if(iscntrl(c)) syntax[ptr - start] = SyntaxTypeOther;
        }

        if(!c) break;

        ptr++;
    }
}

static void parseSyntaxColor(Code* code)
{
    memset(code->syntax, SyntaxTypeVar, TIC_CODE_SIZE);

    tic_mem* tic = code->tic;

    const tic_script_config* config = tic_core_script_config(tic);

    parseCode(config, code->src, code->syntax);
}

static char* getLineByPos(Code* code, char* pos)
{
    char* text = code->src;
    char* line = text;

    while(text < pos)
        if(*text++ == '\n')
            line = text;

    return line;
}

static char* getLine(Code* code)
{
    return getLineByPos(code, code->cursor.position);
}

static char* getPrevLine(Code* code)
{
    char* text = code->src;
    char* pos = code->cursor.position;
    char* prevLine = text;
    char* line = text;

    while(text < pos)
        if(*text++ == '\n')
        {
            prevLine = line;
            line = text;
        }

    return prevLine;
}

static char* getNextLineByPos(Code* code, char* pos)
{
    while(*pos && *pos++ != '\n');

    return pos;
}

static char* getNextLine(Code* code)
{
    return getNextLineByPos(code, code->cursor.position);
}

static s32 getLineSize(const char* line)
{
    s32 size = 0;
    while(*line != '\n' && *line++) size++;

    return size;
}

static void updateColumn(Code* code)
{
    code->cursor.column = code->cursor.position - getLine(code);
}

static void updateCursorPosition(Code* code, char* position)
{
    code->cursor.position = position;
    updateColumn(code);
    updateEditor(code);
}

static void setCursorPosition(Code* code, s32 cx, s32 cy)
{
    s32 x = 0;
    s32 y = 0;
    char* pointer = code->src;

    while(*pointer)
    {
        if(y == cy && x == cx)
        {
            updateCursorPosition(code, pointer);
            return;
        }

        if(*pointer == '\n')
        {
            if(y == cy && cx > x)
            {
                updateCursorPosition(code, pointer);
                return;
            }

            x = 0;
            y++;
        }
        else x++;

        pointer++;
    }

    updateCursorPosition(code, pointer);
}

static void upLine(Code* code)
{
    char* prevLine = getPrevLine(code);
    size_t prevSize = getLineSize(prevLine);
    size_t size = code->cursor.column;

    code->cursor.position = prevLine + (prevSize > size ? size : prevSize);
}

static void downLine(Code* code)
{
    char* nextLine = getNextLine(code);
    size_t nextSize = getLineSize(nextLine);
    size_t size = code->cursor.column;

    code->cursor.position = nextLine + (nextSize > size ? size : nextSize);
}

static void leftColumn(Code* code)
{
    char* start = code->src;

    if(code->cursor.position > start)
    {
        code->cursor.position--;
        updateColumn(code);
    }
}

static void rightColumn(Code* code)
{
    if(*code->cursor.position)
    {
        code->cursor.position++;
        updateColumn(code);
    }
}

static void leftWord(Code* code)
{
    const char* start = code->src;
    char* pos = code->cursor.position-1;

    if(pos > start)
    {
        if(isalnum_(*pos)) while(pos > start && isalnum_(*(pos-1))) pos--;
        else while(pos > start && !isalnum_(*(pos-1))) pos--;

        code->cursor.position = pos;

        updateColumn(code);
    }
}

static void rightWord(Code* code)
{
    const char* end = code->src + strlen(code->src);
    char* pos = code->cursor.position;

    if(pos < end)
    {
        if(isalnum_(*pos)) while(pos < end && isalnum_(*pos)) pos++;
        else while(pos < end && !isalnum_(*pos)) pos++;

        code->cursor.position = pos;
        updateColumn(code);
    }
}

static void goHome(Code* code)
{
    code->cursor.position = getLine(code);

    updateColumn(code);
}

static void goEnd(Code* code)
{
    char* line = getLine(code);
    code->cursor.position = line + getLineSize(line);

    updateColumn(code);
}

static void goCodeHome(Code *code)
{
    code->cursor.position = code->src;

    updateColumn(code);
}

static void goCodeEnd(Code *code)
{
    code->cursor.position = code->src + strlen(code->src);

    updateColumn(code);
}

static void pageUp(Code* code)
{
    s32 column = 0;
    s32 line = 0;
    getCursorPosition(code, &column, &line);
    setCursorPosition(code, column, line > TEXT_BUFFER_HEIGHT ? line - TEXT_BUFFER_HEIGHT : 0);
}

static void pageDown(Code* code)
{
    s32 column = 0;
    s32 line = 0;
    getCursorPosition(code, &column, &line);
    s32 lines = getLinesCount(code);
    setCursorPosition(code, column, line < lines - TEXT_BUFFER_HEIGHT ? line + TEXT_BUFFER_HEIGHT : lines);
}

static bool replaceSelection(Code* code)
{
    char* pos = code->cursor.position;
    char* sel = code->cursor.selection;

    if(sel && sel != pos)
    {
        char* start = MIN(sel, pos);
        char* end = MAX(sel, pos);

        memmove(start, end, strlen(end) + 1);

        code->cursor.position = start;
        code->cursor.selection = NULL;

        history(code);

        parseSyntaxColor(code);

        return true;
    }

    return false;
}

static void deleteChar(Code* code)
{
    if(!replaceSelection(code))
    {
        char* pos = code->cursor.position;
        memmove(pos, pos + 1, strlen(pos));
        history(code);
        parseSyntaxColor(code);
    }
}

static void backspaceChar(Code* code)
{
    if(!replaceSelection(code) && code->cursor.position > code->src)
    {
        char* pos = --code->cursor.position;
        memmove(pos, pos + 1, strlen(pos));
        history(code);
        parseSyntaxColor(code);
    }
}

static void deleteWord(Code* code)
{
    const char* end = code->src + strlen(code->src);
    char* pos = code->cursor.position;

    if(pos < end)
    {
        if(isalnum_(*pos)) while(pos < end && isalnum_(*pos)) pos++;
        else while(pos < end && !isalnum_(*pos)) pos++;
        memmove(code->cursor.position, pos, strlen(pos) + 1);
        history(code);
        parseSyntaxColor(code);
    }
}

static void backspaceWord(Code* code)
{
    const char* start = code->src;
    char* pos = code->cursor.position-1;

    if(pos > start)
    {
        if(isalnum_(*pos)) while(pos > start && isalnum_(*(pos-1))) pos--;
        else while(pos > start && !isalnum_(*(pos-1))) pos--;
        memmove(pos, code->cursor.position, strlen(code->cursor.position) + 1);
        code->cursor.position = pos;
        history(code);
        parseSyntaxColor(code);
    }
}

static void inputSymbolBase(Code* code, char sym)
{
    if (strlen(code->src) >= sizeof(tic_code))
        return;

    char* pos = code->cursor.position;

    memmove(pos + 1, pos, strlen(pos)+1);

    *code->cursor.position++ = sym;

    history(code);

    updateColumn(code);

    parseSyntaxColor(code);
}

static void inputSymbol(Code* code, char sym)
{
    replaceSelection(code);

    inputSymbolBase(code, sym);
}

static void newLine(Code* code)
{
    if(!replaceSelection(code))
    {
        char* ptr = getLine(code);
        size_t size = 0;

        while(*ptr == '\t' || *ptr == ' ') ptr++, size++;

        if(ptr > code->cursor.position)
            size -= ptr - code->cursor.position;

        inputSymbol(code, '\n');

        for(size_t i = 0; i < size; i++)
            inputSymbol(code, '\t');
    }
}

static void selectAll(Code* code)
{
    code->cursor.selection = code->src;
        code->cursor.position = code->cursor.selection + strlen(code->cursor.selection);
}

static void copyToClipboard(Code* code)
{
    char* pos = code->cursor.position;
    char* sel = code->cursor.selection;

    char* start = NULL;
    size_t size = 0;

    if(sel && sel != pos)
    {
        start = MIN(sel, pos);
        size = MAX(sel, pos) - start;
    }
    else
    {
        start = getLine(code);
        size = getNextLine(code) - start;
    }

    char* clipboard = (char*)malloc(size+1);

    if(clipboard)
    {
        memcpy(clipboard, start, size);
        clipboard[size] = '\0';
        getSystem()->setClipboardText(clipboard);
        free(clipboard);
    }
}

static void cutToClipboard(Code* code)
{
    if(code->cursor.selection == NULL || code->cursor.position == code->cursor.selection)
    {
        code->cursor.position = getLine(code);
        code->cursor.selection = getNextLine(code);
    }
    
    copyToClipboard(code);
    replaceSelection(code);
    history(code);
}

static void copyFromClipboard(Code* code)
{
    if(getSystem()->hasClipboardText())
    {
        char* clipboard = getSystem()->getClipboardText();

        if(clipboard)
        {
            removeInvalidChars(clipboard);
            size_t size = strlen(clipboard);

            if(size)
            {
                replaceSelection(code);

                char* pos = code->cursor.position;

                // cut clipboard code if overall code > max code size
                {
                    size_t codeSize = strlen(code->src);

                    if (codeSize + size > sizeof(tic_code))
                    {
                        size = sizeof(tic_code) - codeSize;
                        clipboard[size] = '\0';
                    }
                }

                memmove(pos + size, pos, strlen(pos) + 1);
                memcpy(pos, clipboard, size);

                code->cursor.position += size;

                history(code);

                parseSyntaxColor(code);
            }

            getSystem()->freeClipboardText(clipboard);
        }
    }
}

static void update(Code* code)
{
    updateEditor(code);
    parseSyntaxColor(code);
}

static void undo(Code* code)
{
    history_undo(code->history);
    history_undo(code->cursorHistory);

    update(code);
}

static void redo(Code* code)
{
    history_redo(code->history);
    history_redo(code->cursorHistory);

    update(code);
}

static void doTab(Code* code, bool shift, bool crtl)
{
    char* cursor_position = code->cursor.position;
    char* cursor_selection = code->cursor.selection;
    
    bool has_selection = cursor_selection && cursor_selection != cursor_position;
    bool modifier_key_pressed = shift || crtl;
    
    if(has_selection || modifier_key_pressed)
    {
        char* start;
        char* end;
        
        bool changed = false;
        
        if(cursor_selection) {
            start = MIN(cursor_selection, cursor_position);
            end = MAX(cursor_selection, cursor_position);
        } else {
            start = end = cursor_position;
        }

        char* line = start = getLineByPos(code, start);

        while(line)
        {
            if(shift)
            {
                if(*line == '\t' || *line == ' ')
                {
                    memmove(line, line + 1, strlen(line)+1);
                    end--;
                    changed = true;
                }
            }
            else
            {
                memmove(line + 1, line, strlen(line)+1);
                *line = '\t';
                end++;

                changed = true;
            }
            
            line = getNextLineByPos(code, line);
            if(line >= end) break;
        }
        
        if(changed) {
            
            if(has_selection) {
                code->cursor.position = start;
                code->cursor.selection = end;
            }
            else if (start <= end) code->cursor.position = end;
            
            history(code);
            parseSyntaxColor(code);
        }
    }
    else inputSymbolBase(code, '\t');
}

static void setFindMode(Code* code)
{
    if(code->cursor.selection)
    {
        const char* end = MAX(code->cursor.position, code->cursor.selection);
        const char* start = MIN(code->cursor.position, code->cursor.selection);
        size_t len = end - start;

        if(len > 0 && len < sizeof code->popup.text - 1)
        {
            memset(code->popup.text, 0, sizeof code->popup.text);
            memcpy(code->popup.text, start, len);
        }
    }
}

static void setGotoMode(Code* code)
{
    code->jump.line = -1;
}

static s32 funcCompare(const void* a, const void* b)
{
    const tic_outline_item* item1 = (const tic_outline_item*)a;
    const tic_outline_item* item2 = (const tic_outline_item*)b;

    return strcmp(item1->pos, item2->pos);
}

static void normalizeScroll(Code* code)
{
    if(code->scroll.x < 0) code->scroll.x = 0;
    if(code->scroll.y < 0) code->scroll.y = 0;
    else
    {
        s32 lines = getLinesCount(code);
        if(code->scroll.y > lines) code->scroll.y = lines;
    }
}

static void centerScroll(Code* code)
{
    s32 col, line;
    getCursorPosition(code, &col, &line);
    code->scroll.x = col - CODE_EDITOR_WIDTH / getFontWidth(code) / 2;
    code->scroll.y = line - TEXT_BUFFER_HEIGHT / 2;

    normalizeScroll(code);
}

static void updateOutlineCode(Code* code)
{
    tic_mem* tic = code->tic;

    const tic_outline_item* item = code->outline.items + code->outline.index;

    if(item && item->pos)
    {
        code->cursor.position = (char*)item->pos;
        code->cursor.selection = (char*)item->pos + item->size;
    }
    else
    {
        code->cursor.position = code->src;
        code->cursor.selection = NULL;
    }

    centerScroll(code);
    updateEditor(code);
}

static bool isFilterMatch(const char* buffer, const char* filter)
{
    while(*buffer)
    {
        if(tolower(*buffer) == tolower(*filter))
            filter++;
        buffer++;
    }

    return *filter == 0;
}

static void drawFilterMatch(Code *code, s32 x, s32 y, const char* orig, const char* filter)
{
    while(*orig)
    {
        bool match = tolower(*orig) == tolower(*filter);
        u8 color = match ? tic_color_3 : tic_color_12;

        if(code->shadowText)
            drawChar(code->tic, *orig, x+1, y+1, tic_color_0);

        drawChar(code->tic, *orig, x, y, color);
        x += getFontWidth(code);
        if(match)
            filter++;

        orig++;
    }
}

static void initOutlineMode(Code* code)
{
    tic_mem* tic = code->tic;

    if(code->outline.items)
    {
        free(code->outline.items);
        code->outline.items = NULL;
        code->outline.size = 0;
    }

    const tic_script_config* config = tic_core_script_config(tic);

    if(config->getOutline)
    {
        s32 size = 0;
        const tic_outline_item* items = config->getOutline(code->src, &size);

        if(items)
        {
            char filter[STUDIO_TEXT_BUFFER_WIDTH] = {0};
            strncpy(filter, code->popup.text, sizeof(filter));

            for(s32 i = 0; i < size; i++)
            {
                const tic_outline_item* item = items + i;

                char buffer[STUDIO_TEXT_BUFFER_WIDTH] = {0};
                memcpy(buffer, item->pos, MIN(item->size, sizeof(buffer)));

                if(code->syntax[item->pos - code->src] == SyntaxTypeComment)
                    continue;

                if(*filter && !isFilterMatch(buffer, filter))
                    continue;

                s32 last = code->outline.size++;
                code->outline.items = realloc(code->outline.items, code->outline.size * sizeof(tic_outline_item));
                code->outline.items[last] = *item;
            }
        }
    }
}

static void setOutlineMode(Code* code)
{
    code->outline.index = 0;

    initOutlineMode(code);

    qsort(code->outline.items, code->outline.size, sizeof(tic_outline_item), funcCompare);
    updateOutlineCode(code);
}

static void setASCIIMode(Code* code)
{
	//code->outline.index = 0;

	//initOutlineMode(code);

	//qsort(code->outline.items, code->outline.size, sizeof(tic_outline_item), funcCompare);
	//updateOutlineCode(code);
	updateEditor(code);
}

static void setCodeMode(Code* code, s32 mode)
{
    if(code->mode != mode)
    {
        strcpy(code->popup.text, "");

        code->popup.prevPos = code->cursor.position;
        code->popup.prevSel = code->cursor.selection;

        switch(mode)
        {
        case TEXT_FIND_MODE: setFindMode(code); break;
        case TEXT_GOTO_MODE: setGotoMode(code); break;
        case TEXT_OUTLINE_MODE: setOutlineMode(code); break;
		case TEXT_ASCII_MODE: setASCIIMode(code); break;
        default: break;
        }

        code->mode = mode;
    }
}

static void commentLine(Code* code)
{
    const char* comment = tic_core_script_config(code->tic)->singleComment;
    size_t size = strlen(comment);

    char* line = getLine(code);

    const char* end = line + getLineSize(line);

    while((*line == ' ' || *line == '\t') && line < end) line++;

    if(memcmp(line, comment, size))
    {
        if (strlen(code->src) + size >= sizeof(tic_code))
            return;

        memmove(line + size, line, strlen(line)+1);
        memcpy(line, comment, size);

        if(code->cursor.position > line)
            code->cursor.position += size;
    }
    else
    {
        memmove(line, line + size, strlen(line + size)+1);

        if(code->cursor.position > line + size)
            code->cursor.position -= size;
    }

    code->cursor.selection = NULL;  

    history(code);

    parseSyntaxColor(code);
}

static void processKeyboard(Code* code)
{
    tic_mem* tic = code->tic;

    if(tic->ram.input.keyboard.data == 0) return;

    bool usedClipboard = true;

    switch(getClipboardEvent())
    {
    case TIC_CLIPBOARD_CUT: cutToClipboard(code); break;
    case TIC_CLIPBOARD_COPY: copyToClipboard(code); break;
    case TIC_CLIPBOARD_PASTE: copyFromClipboard(code); break;
    default: usedClipboard = false; break;
    }

    bool shift = tic_api_key(tic, tic_key_shift);
    bool ctrl = tic_api_key(tic, tic_key_ctrl);
    bool alt = tic_api_key(tic, tic_key_alt);

    bool changedSelection = false;
    if(keyWasPressed(tic_key_up)
        || keyWasPressed(tic_key_down)
        || keyWasPressed(tic_key_left)
        || keyWasPressed(tic_key_right)
        || keyWasPressed(tic_key_home)
        || keyWasPressed(tic_key_end)
        || keyWasPressed(tic_key_pageup)
        || keyWasPressed(tic_key_pagedown))
    {
        if(!shift) code->cursor.selection = NULL;
        else if(code->cursor.selection == NULL) code->cursor.selection = code->cursor.position;
        changedSelection = true;
    }

    bool usedKeybinding = true;

    if(ctrl)
    {
        if(keyWasPressed(tic_key_left))             leftWord(code);
        else if(keyWasPressed(tic_key_right))       rightWord(code);
        else if(keyWasPressed(tic_key_tab))         doTab(code, shift, ctrl);
        else if(keyWasPressed(tic_key_a))           selectAll(code);
        else if(keyWasPressed(tic_key_z))           undo(code);
        else if(keyWasPressed(tic_key_y))           redo(code);
        else if(keyWasPressed(tic_key_f))           setCodeMode(code, TEXT_FIND_MODE);
        else if(keyWasPressed(tic_key_g))           setCodeMode(code, TEXT_GOTO_MODE);
        else if(keyWasPressed(tic_key_o))           setCodeMode(code, TEXT_OUTLINE_MODE);
		else if(keyWasPressed(tic_key_i))			setCodeMode(code, TEXT_ASCII_MODE);
        else if(keyWasPressed(tic_key_slash))       commentLine(code);
        else if(keyWasPressed(tic_key_home))        goCodeHome(code);
        else if(keyWasPressed(tic_key_end))         goCodeEnd(code);
        else if(keyWasPressed(tic_key_delete))      deleteWord(code);
        else if(keyWasPressed(tic_key_backspace))   backspaceWord(code);
        else                                        usedKeybinding = false;
    }
    else if(alt)
    {
        if(keyWasPressed(tic_key_left))         leftWord(code);
        else if(keyWasPressed(tic_key_right))   rightWord(code);
        else                                    usedKeybinding = false;
    }
    else
    {
        if(keyWasPressed(tic_key_up))               upLine(code);
        else if(keyWasPressed(tic_key_down))        downLine(code);
        else if(keyWasPressed(tic_key_left))        leftColumn(code);
        else if(keyWasPressed(tic_key_right))       rightColumn(code);
        else if(keyWasPressed(tic_key_home))        goHome(code);
        else if(keyWasPressed(tic_key_end))         goEnd(code);
        else if(keyWasPressed(tic_key_pageup))      pageUp(code);
        else if(keyWasPressed(tic_key_pagedown))    pageDown(code);
        else if(keyWasPressed(tic_key_delete))      deleteChar(code);
        else if(keyWasPressed(tic_key_backspace))   backspaceChar(code);
        else if(keyWasPressed(tic_key_return))      newLine(code);
        else if(keyWasPressed(tic_key_tab))         doTab(code, shift, ctrl);
        else                                        usedKeybinding = false;
    }

    if(usedClipboard || changedSelection || usedKeybinding) updateEditor(code);
}

static void processMouse(Code* code)
{
    tic_mem* tic = code->tic;

    tic_rect rect = {BOOKMARK_WIDTH, TOOLBAR_SIZE, CODE_EDITOR_WIDTH, CODE_EDITOR_HEIGHT};

    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_ibeam);

        if(code->scroll.active)
        {
            if(checkMouseDown(&rect, tic_mouse_right))
            {
                code->scroll.x = (code->scroll.start.x - getMouseX()) / getFontWidth(code);
                code->scroll.y = (code->scroll.start.y - getMouseY()) / STUDIO_TEXT_HEIGHT;

                normalizeScroll(code);
            }
            else code->scroll.active = false;
        }
        else
        {
            if(checkMouseDown(&rect, tic_mouse_left))
            {
                s32 mx = getMouseX();
                s32 my = getMouseY();

                s32 x = (mx - rect.x) / getFontWidth(code);
                s32 y = (my - rect.y) / STUDIO_TEXT_HEIGHT;

                char* position = code->cursor.position;
                setCursorPosition(code, x + code->scroll.x, y + code->scroll.y);

                if(tic_api_key(tic, tic_key_shift))
                {
                    code->cursor.selection = code->cursor.position;
                    code->cursor.position = position;
                }
                else if(!code->cursor.mouseDownPosition)
                {
                    code->cursor.selection = code->cursor.position;
                    code->cursor.mouseDownPosition = code->cursor.position;
                }
            }
            else
            {
                if(code->cursor.mouseDownPosition == code->cursor.position)
                    code->cursor.selection = NULL;

                code->cursor.mouseDownPosition = NULL;
            }

            if(checkMouseDown(&rect, tic_mouse_right))
            {
                code->scroll.active = true;

                code->scroll.start.x = getMouseX() + code->scroll.x * getFontWidth(code);
                code->scroll.start.y = getMouseY() + code->scroll.y * STUDIO_TEXT_HEIGHT;
            }

        }
    }
}

static void textEditTick(Code* code)
{
    tic_mem* tic = code->tic;

    // process scroll
    {
        tic80_input* input = &code->tic->ram.input;

        if(input->mouse.scrolly)
        {
            enum{Scroll = 3};
            s32 delta = input->mouse.scrolly > 0 ? -Scroll : Scroll;
            code->scroll.y += delta;

            normalizeScroll(code);
        }
    }

    processKeyboard(code);

    if(!tic_api_key(tic, tic_key_ctrl) && !tic_api_key(tic, tic_key_alt))
    {
        char sym = getKeyboardText();

        if(sym)
        {
            inputSymbol(code, sym+(code->altFont ? 128:0));
            updateEditor(code);
        }
    }

    processMouse(code);

    tic_api_cls(code->tic, getConfig()->theme.code.bg);

    drawCode(code, true);   
    drawStatus(code);
}

static void drawPopupBar(Code* code, const char* title)
{
    enum {TextX = BOOKMARK_WIDTH, TextY = TOOLBAR_SIZE + 1};

    tic_api_rect(code->tic, 0, TOOLBAR_SIZE, TIC80_WIDTH, TIC_FONT_HEIGHT + 1, tic_color_14);
	/*
    if(code->shadowText)
        tic_api_print(code->tic, title, TextX+1, TextY+1, tic_color_0, true, 1, code->altFont);
	*/
    tic_api_print(code->tic, title, TextX, TextY, tic_color_12, 1);
	/*
    if(code->shadowText)
        tic_api_print(code->tic, code->popup.text, TextX + (s32)strlen(title) * getFontWidth(code) + 1, TextY+1, tic_color_0, true, 1, code->altFont);
	*/
    tic_api_print(code->tic, code->popup.text, TextX + (s32)strlen(title) * getFontWidth(code), TextY, tic_color_12, 1);

    drawCursor(code, TextX+(s32)(strlen(title) + strlen(code->popup.text)) * getFontWidth(code), TextY, ' ');
}

static void updateFindCode(Code* code, char* pos)
{
    if(pos)
    {
        code->cursor.position = pos;
        code->cursor.selection = pos + strlen(code->popup.text);

        centerScroll(code);
        updateEditor(code);
    }
}

static char* upStrStr(const char* start, const char* from, const char* substr)
{
    const char* ptr = from-1;
    size_t len = strlen(substr);

    if(len > 0)
    {
        while(ptr >= start)
        {
            if(memcmp(ptr, substr, len) == 0)
                return (char*)ptr;

            ptr--;
        }
    }

    return NULL;
}

static char* downStrStr(const char* start, const char* from, const char* substr)
{
    return strstr(from, substr);
}

static void textFindTick(Code* code)
{
    if(keyWasPressed(tic_key_return)) setCodeMode(code, TEXT_EDIT_MODE);
    else if(keyWasPressed(tic_key_up)
        || keyWasPressed(tic_key_down)
        || keyWasPressed(tic_key_left)
        || keyWasPressed(tic_key_right))
    {
        if(*code->popup.text)
        {
            bool reverse = keyWasPressed(tic_key_up) || keyWasPressed(tic_key_left);
            char* (*func)(const char*, const char*, const char*) = reverse ? upStrStr : downStrStr;
            char* from = reverse ? MIN(code->cursor.position, code->cursor.selection) : MAX(code->cursor.position, code->cursor.selection);
            char* pos = func(code->src, from, code->popup.text);
            updateFindCode(code, pos);
        }
    }
    else if(keyWasPressed(tic_key_backspace))
    {
        if(*code->popup.text)
        {
            code->popup.text[strlen(code->popup.text)-1] = '\0';
            updateFindCode(code, strstr(code->src, code->popup.text));
        }
    }

    char sym = getKeyboardText();

    if(sym)
    {
        if(strlen(code->popup.text) + 1 < sizeof code->popup.text)
        {
            char str[] = {sym , 0};
            strcat(code->popup.text, str);
            updateFindCode(code, strstr(code->src, code->popup.text));
        }
    }

    tic_api_cls(code->tic, getConfig()->theme.code.bg);

    drawCode(code, false);
    drawPopupBar(code, "FIND:");
    drawStatus(code);
}

static void updateGotoCode(Code* code)
{
    s32 line = atoi(code->popup.text);

    if(line) line--;

    s32 count = getLinesCount(code);

    if(line > count) line = count;

    code->cursor.selection = NULL;
    setCursorPosition(code, 0, line);

    code->jump.line = line;

    centerScroll(code);
    updateEditor(code);
}

static void textGoToTick(Code* code)
{
    tic_mem* tic = code->tic;

    if(keyWasPressed(tic_key_return))
    {
        if(*code->popup.text)
            updateGotoCode(code);

        setCodeMode(code, TEXT_EDIT_MODE);
    }
    else if(keyWasPressed(tic_key_backspace))
    {
        if(*code->popup.text)
        {
            code->popup.text[strlen(code->popup.text)-1] = '\0';
            updateGotoCode(code);
        }
    }

    char sym = getKeyboardText();

    if(sym)
    {
        if(strlen(code->popup.text)+1 < sizeof code->popup.text && sym >= '0' && sym <= '9')
        {
            char str[] = {sym, 0};
            strcat(code->popup.text, str);
            updateGotoCode(code);
        }
    }

    tic_api_cls(tic, getConfig()->theme.code.bg);

    if(code->jump.line >= 0)
        tic_api_rect(tic, 0, (code->jump.line - code->scroll.y) * (TIC_FONT_HEIGHT+1) + TOOLBAR_SIZE,
            TIC80_WIDTH, TIC_FONT_HEIGHT+2, getConfig()->theme.code.select);

    drawCode(code, false);
    drawPopupBar(code, "GOTO:");
    drawStatus(code);
}

static void drawASCIItable(Code* code, s32 x, s32 y)
{
    tic_rect rect = {x, y, 16*(TIC_FONT_WIDTH+1), 16*(TIC_FONT_HEIGHT+1)};

	tic_api_rect(code->tic, rect.x - 1, rect.y-6, rect.w + 1, rect.h+6, tic_color_14);
	tic_api_print(code->tic, "ASCII index:",x,y-6,tic_color_13,1);
	
	for (u8 yyy = 0; yyy < 16; yyy++)
	{
		for (u8 xxx = 0; xxx < 16; xxx++)
		{
			tic_rect ASCIIrect = { rect.x + xxx * (TIC_FONT_WIDTH + 1),rect.y + yyy * (TIC_FONT_HEIGHT + 1),7,7 };
			if (checkMousePos(&ASCIIrect))
			{
				char buf[] = "#999";
				sprintf(buf, "#%03i", xxx + yyy * 16);
				tic_api_print(code->tic, buf, x+72, y - 6, tic_color_13, 1);

				tic_api_rect(code->tic, xxx*(TIC_FONT_WIDTH + 1) + rect.x, yyy*(TIC_FONT_HEIGHT + 1) + rect.y, 7, 7, tic_color_2);
			}
			tic_api_print(code->tic, (char[]) { xxx + (yyy * 16), '\0' }, xxx*(TIC_FONT_WIDTH + 1) + rect.x, yyy*(TIC_FONT_HEIGHT + 1) + rect.y, tic_color_12, 1);
			
			

			if (checkMouseClick(&ASCIIrect,tic_mouse_left))
			{
				inputSymbol(code, xxx + yyy * 16);

			}
			//drawChar(code->tic, char[](xxx + yyy), xxx*TIC_FONT_WIDTH + rect.x, yyy*TIC_FONT_HEIGHT + rect.y, tic_color_12);
		}
	}
	tic_rect outrect = { 0,0,TIC80_WIDTH,TIC80_HEIGHT };
	if (!checkMousePos(&rect))
		if (checkMouseClick(&outrect, tic_mouse_left))
			code->mode = TEXT_EDIT_MODE;

	/*
    if(checkMousePos(&rect))
    {
        s32 mx = getMouseY() - rect.y;
        mx /= STUDIO_TEXT_HEIGHT;
		/*
        if(mx < code->outline.size && code->outline.items[mx].pos)
        {
            setCursor(tic_cursor_hand);

            if(checkMouseDown(&rect, tic_mouse_left))
            {
                //code->outline.index = mx;
                //updateOutlineCode(code);

            }

            if(checkMouseClick(&rect, tic_mouse_left))
                setCodeMode(code, TEXT_EDIT_MODE);
        }
		
		//if (!checkMouseClick(&rect, tic_mouse_left))
		//	setCodeMode(code, TEXT_EDIT_MODE);
    }
*/
    
/*
    y++;

    char filter[STUDIO_TEXT_BUFFER_WIDTH] = {0};
    strncpy(filter, code->popup.text, sizeof(filter));

    if(code->outline.items)
    {
        tic_api_rect(code->tic, rect.x - 1, rect.y + code->outline.index*STUDIO_TEXT_HEIGHT,
            rect.w + 1, TIC_FONT_HEIGHT + 2, tic_color_2);

        for(s32 i = 0; i < code->outline.size; i++)
        {
            const tic_outline_item* ptr = &code->outline.items[i];

            char orig[STUDIO_TEXT_BUFFER_WIDTH] = {0};
            strncpy(orig, ptr->pos, MIN(ptr->size, sizeof(orig)));

            drawFilterMatch(code, x, y, orig, filter);

            ptr++;
            y += STUDIO_TEXT_HEIGHT;
        }
    }
    else
    {
        tic_api_print(code->tic, "(empty)", x, y, tic_color_12, true, 1, code->altFont);
    }
*/
}

static void drawOutlineBar(Code* code, s32 x, s32 y)
{
    tic_rect rect = {x, y, TIC80_WIDTH - x, TIC80_HEIGHT - y};

    if(checkMousePos(&rect))
    {
        s32 mx = getMouseY() - rect.y;
        mx /= STUDIO_TEXT_HEIGHT;

        if(mx < code->outline.size && code->outline.items[mx].pos)
        {
            setCursor(tic_cursor_hand);

            if(checkMouseDown(&rect, tic_mouse_left))
            {
                code->outline.index = mx;
                updateOutlineCode(code);

            }

            if(checkMouseClick(&rect, tic_mouse_left))
                setCodeMode(code, TEXT_EDIT_MODE);
        }
    }

    tic_api_rect(code->tic, rect.x-1, rect.y, rect.w+1, rect.h, tic_color_14);

    y++;

    char filter[STUDIO_TEXT_BUFFER_WIDTH] = {0};
    strncpy(filter, code->popup.text, sizeof(filter));

    if(code->outline.items)
    {
        tic_api_rect(code->tic, rect.x - 1, rect.y + code->outline.index*STUDIO_TEXT_HEIGHT,
            rect.w + 1, TIC_FONT_HEIGHT + 2, tic_color_2);

        for(s32 i = 0; i < code->outline.size; i++)
        {
            const tic_outline_item* ptr = &code->outline.items[i];

            char orig[STUDIO_TEXT_BUFFER_WIDTH] = {0};
            strncpy(orig, ptr->pos, MIN(ptr->size, sizeof(orig)));

            drawFilterMatch(code, x, y, orig, filter);

            ptr++;
            y += STUDIO_TEXT_HEIGHT;
        }
    }
    else
    {
        /*if(code->shadowText)
            tic_api_print(code->tic, "(empty)", x+1, y+1, tic_color_0, true, 1, code->altFont);
		*/
        tic_api_print(code->tic, "(empty)", x, y, tic_color_12, 1);
    }
}
static void textASCIITick(Code* code)
{

	tic_api_cls(code->tic, getConfig()->theme.code.bg);

	//drawCode(code, false);
	drawCode(code, true);
	//drawPopupBar(code, "ASCII");
	drawStatus(code);
	drawASCIItable(code, TIC80_WIDTH - 16 * (TIC_FONT_WIDTH + 1), 2 * (TIC_FONT_HEIGHT + 1));
}

static void textOutlineTick(Code* code)
{
    if(keyWasPressed(tic_key_up))
    {
        if(code->outline.index > 0)
        {
            code->outline.index--;
            updateOutlineCode(code);
        }
    }
    else if(keyWasPressed(tic_key_down))
    {
        if(code->outline.index < code->outline.size - 1 && code->outline.items[code->outline.index + 1].pos)
        {
            code->outline.index++;
            updateOutlineCode(code);
        }
    }
    else if(keyWasPressed(tic_key_return))
    {
        updateOutlineCode(code);
        setCodeMode(code, TEXT_EDIT_MODE);
    }
    else if(keyWasPressed(tic_key_backspace))
    {
        if(*code->popup.text)
        {
            code->popup.text[strlen(code->popup.text)-1] = '\0';
            setOutlineMode(code);
        }
    }

    char sym = getKeyboardText();

    if(sym)
    {
        if(strlen(code->popup.text) + 1 < sizeof code->popup.text)
        {
            char str[] = {sym, 0};
            strcat(code->popup.text, str);
            setOutlineMode(code);
        }
    }

    tic_api_cls(code->tic, getConfig()->theme.code.bg);

    drawCode(code, false);
    drawPopupBar(code, "FUNC:");
    drawStatus(code);
    drawOutlineBar(code, TIC80_WIDTH - 13 * TIC_FONT_WIDTH, 2*(TIC_FONT_HEIGHT+1));
}


static void drawFontButton(Code* code, s32 x, s32 y)
{
	tic_mem* tic = code->tic;

    enum {Size = TIC_FONT_WIDTH};
    tic_rect rect = {x, y, Size, Size};

    bool over = false;
    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);

        showTooltip("Extended symbols");

        over = true;

        if(checkMouseClick(&rect, tic_mouse_left))
        {
            code->altFont = !code->altFont;
        }
    }

    drawChar(tic, 'F', x, y, over ? (code->altFont ? tic_color_7 : tic_color_14) : (code->altFont ? tic_color_6 : tic_color_13));
}



static void drawASCIIButton(Code* code, s32 x, s32 y)
{
	tic_mem* tic = code->tic;

	enum { Size = TIC_FONT_WIDTH };
	tic_rect rect = { x, y, Size+1, Size };

	bool over = false;
	if (checkMousePos(&rect))
	{
		setCursor(tic_cursor_hand);

		showTooltip("ASCII Table [ctrl+i]");

		over = true;

		if (checkMouseClick(&rect, tic_mouse_left))
		{
			if (code->mode == TEXT_ASCII_MODE) 
				code->mode = TEXT_EDIT_MODE;
			else
				code->mode = TEXT_ASCII_MODE;
		}
	}

	//drawChar(tic, 'ASCII', x, y, over ? tic_color_14 : tic_color_13);
	if (code->mode == TEXT_ASCII_MODE)
	{
		tic_api_rect(tic, rect.x, rect.y, rect.w, rect.h, tic_color_14);
		tic_api_print(tic, (char[]) { 146, '\0' }, x+1, y + 1, tic_color_0, 1);
		tic_api_print(tic, (char[]) { 146, '\0' }, x+1, y, tic_color_12, 1);
	}
	else {
		tic_api_print(tic, (char[]) { 146, '\0' }, x+1, y, over ? tic_color_14 : tic_color_13, 1);
	}


}

static void drawTABButton(Code* code, s32 x, s32 y)
{
	static const u8 Icon[] =
	{
		0b00001001,
		0b00000101,
		0b11111111,
		0b00000101,
		0b00001001,
		0b00000000,
		0b00000000,
		0b00000000,
	};

	tic_mem* tic = code->tic;

	enum { Size = TIC_FONT_WIDTH };
	tic_rect rect = { x, y, Size, Size };

	bool over = false;
	if (checkMousePos(&rect))
	{
		setCursor(tic_cursor_hand);

		showTooltip("Show tabs");

		over = true;

		if (checkMouseClick(&rect, tic_mouse_left))
		{
			code->show_tabs = !code->show_tabs;
		}
	}
	bool tabs = code->show_tabs;

	drawBitIcon(rect.x, rect.y, Icon, over ? (code->show_tabs ? tic_color_7 : tic_color_14) : (code->show_tabs ? tic_color_6 : tic_color_13));
}
/*
static void drawShadowButton(Code* code, s32 x, s32 y)
{
    tic_mem* tic = code->tic;

    enum {Size = TIC_FONT_WIDTH};
    tic_rect rect = {x, y, Size, Size};

    bool over = false;
    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);

        showTooltip("SHOW SHADOW");

        over = true;

        if(checkMouseClick(&rect, tic_mouse_left))
        {
            code->shadowText = !code->shadowText;
        }
    }

    static const u8 Icon[] =
    {
        0b11110000,
        0b10011000,
        0b10011000,
        0b11111000,
        0b01111000,
        0b00000000,
        0b00000000,
        0b00000000,
    };

    static const u8 ShadowIcon[] =
    {
        0b00000000,
        0b00001000,
        0b00001000,
        0b00001000,
        0b01111000,
        0b00000000,
        0b00000000,
        0b00000000,
    };

    drawBitIcon(x, y, Icon, over && !code->shadowText ? tic_color_14 : tic_color_13);

    if(code->shadowText)
        drawBitIcon(x, y, ShadowIcon, tic_color_0);
}
*/
static void drawCodeToolbar(Code* code)
{
    tic_api_rect(code->tic, 0, 0, TIC80_WIDTH, TOOLBAR_SIZE, tic_color_12);

    static const u8 Icons[] =
    {
        0b00000000,
        0b00100000,
        0b00110000,
        0b00111000,
        0b00110000,
        0b00100000,
        0b00000000,
        0b00000000,

        0b00000000,
        0b00111000,
        0b01000100,
        0b00111000,
        0b00010000,
        0b00010000,
        0b00000000,
        0b00000000,

        0b00000000,
        0b00010000,
        0b00011000,
        0b01111100,
        0b00011000,
        0b00010000,
        0b00000000,
        0b00000000,

        0b00000000,
        0b01111100,
        0b00000000,
        0b01111100,
        0b00000000,
        0b01111100,
        0b00000000,
        0b00000000,
    };

    enum {Count = sizeof Icons / BITS_IN_BYTE};
    enum {Size = 7};

    static const char* Tips[] = {"RUN [ctrl+r]","FIND [ctrl+f]", "GOTO [ctrl+g]", "OUTLINE [ctrl+o]"};

    for(s32 i = 0; i < Count; i++)
    {
        tic_rect rect = {TIC80_WIDTH + (i - Count) * Size, 0, Size, Size};

        bool over = false;
        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            showTooltip(Tips[i]);

            over = true;

            if(checkMouseClick(&rect, tic_mouse_left))
            {
                if (i == TEXT_RUN_CODE)
                {
                    runProject();
                }
                else
                {
                    s32 mode = TEXT_EDIT_MODE + i;

                    if(code->mode == mode) code->escape(code);
                    else setCodeMode(code, mode);
                }
            }
        }

        bool active = i == code->mode - TEXT_EDIT_MODE && i != 0;
        if (active)
        {
            tic_api_rect(code->tic, rect.x, rect.y, rect.w, rect.h, tic_color_14);
            drawBitIcon(rect.x, rect.y + 1, Icons + i * BITS_IN_BYTE, tic_color_0);
        }
        drawBitIcon(rect.x, rect.y, Icons + i*BITS_IN_BYTE, active ? tic_color_12 : (over ? tic_color_14 : tic_color_13));
    }

    drawASCIIButton(code, TIC80_WIDTH - (Count+2) * Size, 1);
    drawFontButton(code, TIC80_WIDTH - (Count+3) * Size, 1);
	drawTABButton(code, TIC80_WIDTH - (Count+5) * Size, 1);
    //drawShadowButton(code, TIC80_WIDTH - (Count+2) * Size, 1);

    drawToolbar(code->tic, false);
}

static void tick(Code* code)
{
    if(code->cursor.delay)
        code->cursor.delay--;

    switch(code->mode)
    {
    case TEXT_RUN_CODE: runProject(); break;
    case TEXT_EDIT_MODE: textEditTick(code); break;
    case TEXT_FIND_MODE: textFindTick(code); break;
    case TEXT_GOTO_MODE: textGoToTick(code); break;
    case TEXT_OUTLINE_MODE: textOutlineTick(code); break;
	case TEXT_ASCII_MODE: textASCIITick(code); break;
    }

    drawCodeToolbar(code);

    code->tickCounter++;
}

static void escape(Code* code)
{
    if(code->mode != TEXT_EDIT_MODE)
    {
        code->cursor.position = code->popup.prevPos;
        code->cursor.selection = code->popup.prevSel;
        code->popup.prevSel = code->popup.prevPos = NULL;

        code->mode = TEXT_EDIT_MODE;

        updateEditor(code);
    }
}

static void onStudioEvent(Code* code, StudioEvent event)
{
    switch(event)
    {
    case TIC_TOOLBAR_CUT: cutToClipboard(code); break;
    case TIC_TOOLBAR_COPY: copyToClipboard(code); break;
    case TIC_TOOLBAR_PASTE: copyFromClipboard(code); break;
    case TIC_TOOLBAR_UNDO: undo(code); break;
    case TIC_TOOLBAR_REDO: redo(code); break;
    }
}

void initCode(Code* code, tic_mem* tic, tic_code* src)
{
    if(code->syntax == NULL)
        code->syntax = malloc(TIC_CODE_SIZE);

    if(code->history) history_delete(code->history);
    if(code->cursorHistory) history_delete(code->cursorHistory);

    *code = (Code)
    {
		.tic = tic,
			.src = src->data,
			.tick = tick,
			.escape = escape,
			.cursor = { {src->data, NULL, 0}, NULL, 0 },
			.scroll = { 0, 0, {0, 0}, false },
			.syntax = code->syntax,
			.tickCounter = 0,
			.history = NULL,
			.cursorHistory = NULL,
			.mode = TEXT_EDIT_MODE,
			.jump = { .line = -1 },
			.popup =
		{
			.prevPos = NULL,
			.prevSel = NULL,
		},
		.outline =
		{
			.items = NULL,
			.size = 0,
			.index = 0,
		},
		.matchedDelim = NULL,
		.show_tabs = false,
        .altFont = getConfig()->theme.code.altFont,
        .shadowText = getConfig()->theme.code.shadow,
        .event = onStudioEvent,
        .update = update,
    };

    code->history = history_create(code->src, sizeof(tic_code));
    code->cursorHistory = history_create(&code->cursor, sizeof code->cursor);

    update(code);
}

void freeCode(Code* code)
{
    free(code->syntax);
    history_delete(code->history);
    history_delete(code->cursorHistory);
    free(code);
}