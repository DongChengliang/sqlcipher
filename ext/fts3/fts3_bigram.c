#include "fts3Int.h"
#if !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_FTS3)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include "fts3_tokenizer.h"

/* Return code if invalid. (xxx_mbtowc) */
#define RET_ILSEQ      -1
/* Return code if only a shift sequence of n bytes was read. (xxx_mbtowc) */
#define RET_TOOFEW(n)  (-2-(n))
/* Return code if invalid. (xxx_wctomb) */
#define RET_ILUNI      -1
/* Return code if output buffer is too small. (xxx_wctomb, xxx_reset) */
#define RET_TOOSMALL   -2

int utf8_mbtowc (wchar_t *pwc, const unsigned char *s, int n)
{
    unsigned char c = s[0];
    
    if (c < 0x80) {
        *pwc = c;
        return 1;
    } else if (c < 0xc2) {
        return RET_ILSEQ;
    } else if (c < 0xe0) {
        if (n < 2)
            return RET_TOOFEW(0);
        if (!((s[1] ^ 0x80) < 0x40))
            return RET_ILSEQ;
        *pwc = ((wchar_t) (c & 0x1f) << 6)
        | (wchar_t) (s[1] ^ 0x80);
        return 2;
    } else if (c < 0xf0) {
        if (n < 3)
            return RET_TOOFEW(0);
        if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40
              && (c >= 0xe1 || s[1] >= 0xa0)))
            return RET_ILSEQ;
        *pwc = ((wchar_t) (c & 0x0f) << 12)
        | ((wchar_t) (s[1] ^ 0x80) << 6)
        | (wchar_t) (s[2] ^ 0x80);
        return 3;
    } else if (c < 0xf8 && sizeof(wchar_t)*8 >= 32) {
        if (n < 4)
            return RET_TOOFEW(0);
        if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40
              && (s[3] ^ 0x80) < 0x40
              && (c >= 0xf1 || s[1] >= 0x90)))
            return RET_ILSEQ;
        *pwc = ((wchar_t) (c & 0x07) << 18)
        | ((wchar_t) (s[1] ^ 0x80) << 12)
        | ((wchar_t) (s[2] ^ 0x80) << 6)
        | (wchar_t) (s[3] ^ 0x80);
        return 4;
    } else if (c < 0xfc && sizeof(wchar_t)*8 >= 32) {
        if (n < 5)
            return RET_TOOFEW(0);
        if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40
              && (s[3] ^ 0x80) < 0x40 && (s[4] ^ 0x80) < 0x40
              && (c >= 0xf9 || s[1] >= 0x88)))
            return RET_ILSEQ;
        *pwc = ((wchar_t) (c & 0x03) << 24)
        | ((wchar_t) (s[1] ^ 0x80) << 18)
        | ((wchar_t) (s[2] ^ 0x80) << 12)
        | ((wchar_t) (s[3] ^ 0x80) << 6)
        | (wchar_t) (s[4] ^ 0x80);
        return 5;
    } else if (c < 0xfe && sizeof(wchar_t)*8 >= 32) {
        if (n < 6)
            return RET_TOOFEW(0);
        if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40
              && (s[3] ^ 0x80) < 0x40 && (s[4] ^ 0x80) < 0x40
              && (s[5] ^ 0x80) < 0x40
              && (c >= 0xfd || s[1] >= 0x84)))
            return RET_ILSEQ;
        *pwc = ((wchar_t) (c & 0x01) << 30)
        | ((wchar_t) (s[1] ^ 0x80) << 24)
        | ((wchar_t) (s[2] ^ 0x80) << 18)
        | ((wchar_t) (s[3] ^ 0x80) << 12)
        | ((wchar_t) (s[4] ^ 0x80) << 6)
        | (wchar_t) (s[5] ^ 0x80);
        return 6;
    } else
        return RET_ILSEQ;
}

int utf8_wctomb (unsigned char *r, wchar_t wc, int n) /* n == 0 is acceptable */
{
    int count;
    if (wc < 0x80)
        count = 1;
    else if (wc < 0x800)
        count = 2;
    else if (wc < 0x10000)
        count = 3;
    else if (wc < 0x200000)
        count = 4;
    else if (wc < 0x4000000)
        count = 5;
    else if (wc <= 0x7fffffff)
        count = 6;
    else
        return RET_ILUNI;
    if (n < count)
        return RET_TOOSMALL;
    switch (count) { /* note: code falls through cases! */
        case 6: r[5] = 0x80 | (wc & 0x3f); wc = wc >> 6; wc |= 0x4000000;
        case 5: r[4] = 0x80 | (wc & 0x3f); wc = wc >> 6; wc |= 0x200000;
        case 4: r[3] = 0x80 | (wc & 0x3f); wc = wc >> 6; wc |= 0x10000;
        case 3: r[2] = 0x80 | (wc & 0x3f); wc = wc >> 6; wc |= 0x800;
        case 2: r[1] = 0x80 | (wc & 0x3f); wc = wc >> 6; wc |= 0xc0;
        case 1: r[0] = wc;
    }
    return count;
}

int utf8_mbstowcs(wchar_t *pwc, const unsigned char *s, int n)
{
    unsigned char *p = s;
    wchar_t *pw = pwc;
    while (*p) {
        if (pw - pwc >= n) {
            break;
        }
        p += utf8_mbtowc(pw++, p, 6);
    }
    int count = (int)(pw - pwc);
    if (count <= n) {
        *pw = L'\0';
    }
    return count;
}

int utf8_wcstombs(unsigned char *r, wchar_t *pwc, int n)
{
    unsigned char *p;
    wchar_t *pw;
    for (p = r, pw = pwc; *pw; pw++) {
        char tmp[6];
        if (p + utf8_wctomb(tmp, *pw, 6) - r > n) {
            break;
        }
        p += utf8_wctomb(p, *pw, 6);
    }
    int count = (int)(p - r);
    if (count <= n) {
        p[0] = '\0';
    }
    return count;
}

typedef struct bigram_tokenizer {
    sqlite3_tokenizer base;
} bigram_tokenizer;

typedef struct bigram_tokenizer_cursor {
    sqlite3_tokenizer_cursor base;
    
    /* 下面都是fts3_tokenizer.h的框架式定义中没有的。是为了implementation而添加的。
     其中重要的是有一个指向input stream（也就是要被分词的文本流，因为要处理wide character string，还需添加一个wc版的input）的指针，然后才能在nextToken时获取之。
     */
    /* 要分词的输入字符串 */
    const char *zInput;
    int nInput;
    
    /* 此次获取的token是第几个（第一个token时该值为0）*/
    int iToken;
    
    /* 此次获取的token */
    char *zToken;
    
    /* 从作为input stream的unicode string（即wcInput）中的何处开始获取下一个token */
    int iOffset_in_wcInput;
    
    /* 要分词的输入字符串的宽字符表示 */
    wchar_t *wcInput;
    
} bigram_tokenizer_cursor;

/* 这个函数是只在初始化tokenizer的时候运行一次吗？还是在每一次启动对一个输入串时都要生成一次？应该是前者。待查。 */
static int bigramCreate(
  int argc, const char * const *argv,
  sqlite3_tokenizer **ppTokenizer
){
    bigram_tokenizer *t;
    t = (bigram_tokenizer *) sqlite3_malloc(sizeof(*t));
    if( t==NULL ) return SQLITE_NOMEM;
    memset(t, 0, sizeof(*t));
    
    *ppTokenizer = &t->base;
    
    return SQLITE_OK;
}

static int bigramDestroy(sqlite3_tokenizer *pTokenizer){
    sqlite3_free(pTokenizer);
    return SQLITE_OK;
}

static int bigramOpen(
  sqlite3_tokenizer *pTokenizer,         /* The tokenizer */
  const char *zInput, int nInput,        /* String to be tokenized */
  sqlite3_tokenizer_cursor **ppCursor    /* OUT: Tokenization cursor */
){
    bigram_tokenizer_cursor *c;
    
    c = (bigram_tokenizer_cursor *) sqlite3_malloc(sizeof(*c));
    if(c == NULL) return SQLITE_NOMEM;
    
    c->zInput = zInput;
    if( zInput==0 ){ // ?? == NULL
        nInput = c->nInput = 0;
    }else if( nInput<0 ){
        nInput = c->nInput = (int)strlen(zInput);
    }else{
        c->nInput = nInput;
    }
    
    c->iOffset_in_wcInput = 0;
    c->iToken = 0;
    c->zToken = NULL;
    
    /* 生成宽字符串c->wcInput。
       分配的buffer的量要足，考虑到zInput中每个byte都解析成一个wide char的可能
     */
    c->wcInput = (wchar_t *)sqlite3_malloc((nInput+1) * sizeof(wchar_t));
    if( c->wcInput==NULL ) return SQLITE_NOMEM;

    utf8_mbstowcs(c->wcInput, zInput, nInput);
    
    *ppCursor = &c->base; // ??
    
    return SQLITE_OK;
}

static int bigramClose(sqlite3_tokenizer_cursor *pCursor){
    /* 清理工作：释放cursor对象生成、存在期间所有动态分配的内存。
       动态分配的内存有三块儿：
         1. 存放bigramNext后获取的一个token的zToken
         2. 存放被分词的字符串的宽字符版本的wcInput
         3. cursor本身
     */
    
    bigram_tokenizer_cursor *c = (bigram_tokenizer_cursor *) pCursor;
    
    /* 释放zToken占据的内存 */
    if (c->zToken != NULL) {
        sqlite3_free(c->zToken);
        c->zToken = NULL;
    }
    
    /* 释放wcInput占据的内存 */
    if(c->wcInput != NULL) {
        sqlite3_free(c->wcInput);
        c->wcInput = NULL;
    }
    
    /* 释放cursor本身占据的内存 */
    sqlite3_free(c);
    
    return SQLITE_OK;
}

wchar_t *global_skipchars = L" \t\n\r\'\",;.()<>[]，。；、（）《》【】";
/* helper function: 一个宽字符是否是“非成词字符”（分词过程中应略过的空白符或者分割符）*/
static int should_skip(wchar_t wc){
    if (wcschr(global_skipchars, wc)) {
        return 1;
    }
    return 0;
}
/* helper function: wide character pos to byte string pos*/
static int wcpos2strpos(wchar_t *wcs, int wcpos)
{
  int wclen = (int)wcslen(wcs);
  /* 注意：应该是wcpos >= wclen， 而不能是wcpos > wclen！
   wcpos == wclen 应该特别处理
   因为这个问题，在iphone模拟器上测试程序时出现了随机错误。
   */
  if (wcpos < 0 || wcpos > wclen) {
      return -1;
  } else if (wcpos == 0) {
      return 0;
  } else if (wcpos == wclen){
      char *buf = (char *)sqlite3_malloc(6*(wclen+1)); // ?? malloc -> sqlite3_malloc
      if (buf == NULL) return SQLITE_NOMEM;
      int ret = (int)utf8_wcstombs(buf, wcs, 6*wclen);
      sqlite3_free(buf);
      return ret;
  } else { /* wcpos within (0, wclen) */
      wchar_t *subwcs = (wchar_t *)sqlite3_malloc( sizeof(wchar_t) * (wcpos+1) );
      if (subwcs == NULL) return SQLITE_NOMEM;
      wcsncpy(subwcs, wcs, wcpos);
      char *buf = (char *)sqlite3_malloc(6*(wcpos+1));
      if (buf == NULL) return SQLITE_NOMEM;
      int ret = (int)utf8_wcstombs(buf, subwcs, 6*wcpos);
      sqlite3_free(subwcs); // added
      sqlite3_free(buf);
      return ret;
  }
}

static int bigramNext(
  sqlite3_tokenizer_cursor *pCursor, /* Cursor returned by bigramOpen */
  const char **ppToken,               /* OUT: *ppToken is the token text */
  int *pnBytes,                       /* OUT: Number of bytes in token */
  int *piStartOffset,                 /* OUT: Starting offset of token */
  int *piEndOffset,                   /* OUT: Ending offset of token */
  int *piPosition                     /* OUT: Position integer of token */
){
    
    /* Core function of this module: get next token from input stream.
     本函数的effect：
     1. 把参数ppToken指向的字符串中填入此次获取的token的内容；在pnBytes指向的int中填入该token的字节长度；
     2. 在piPosition指向的int中填入“此次获取的token是此次对input stream分词过程中获取的所有token中的第几个”的信息
     （Number of tokens returned before this one）的信息。
     3. 获得正确的piStartOffset, piEndOffset。这两项未作处理会出现恶劣的结果。具体原因见代码处注释。
     
     注意：该函数六个参数中，只有第一个参数pCursor是传入信息的，完成任务所需的全部环境、背景信息都应该已经包括在这个cursor中了（如input stream的内容，一些算法的环境配置）。其余五个参数都是传出参数，是为了变相的返回信息而用的。
     */
    
    bigram_tokenizer_cursor *c = (bigram_tokenizer_cursor *) pCursor;
    
    /* 释放上一次获取的token所占据的内存 */
    if(c->zToken != NULL){
        sqlite3_free(c->zToken);
        c->zToken = NULL;
    }    
    
    /* 获取在宽字符串中游动所需的位置信息：pos, len */
    int pos = c->iOffset_in_wcInput;
    int len = (int)wcslen(c->wcInput);
    
    /* 已经到input stream的结尾，对全部输入字符串的分词已经完毕。*/
    if (pos >= len-1) {
        return SQLITE_DONE;
    }

    /* 越过所有不能成词的情形：本字符或者下一字符是非成词字符 */
    while (should_skip(c->wcInput[pos]) || should_skip(c->wcInput[pos+1])) {
        pos++;
        /* 已经到input stream的结尾，对全部输入字符串的分词已经完毕。*/
        if (pos == len-1) {
            return SQLITE_DONE;
        }
    }
    
    /* 获取两个宽字符，组成一个两字词，作为一个token，存为一个宽字符串 */
    wchar_t wctoken[3];
    wctoken[0] = c->wcInput[pos]; wctoken[1] = c->wcInput[pos+1]; wctoken[2] = 0;
    
    /* 将该宽字符串转换为bytes */
    char *buf = (char *)sqlite3_malloc(16); /* 6+6+4 对一个两字的宽字符串空间够了 */
    if (buf == NULL) return SQLITE_NOMEM;
    utf8_wcstombs(buf, wctoken, 12);
    int tokenByteLen = (int)strlen(buf);
    
    /* 返回得到的token */
    *ppToken = c->zToken = buf;
    *pnBytes = tokenByteLen;
    
    /* token的计数加1 */
    *piPosition = c->iToken++;
    
    /* piStartOffset 与 piEndOffset 必须正确处理！否则后果是非常严重的。原因如下：
     fts3_expr.c中的static int getNextToken()中的语句：
     rc = pModule->xNext(pCursor, &zToken, &nToken, &iStart, &iEnd, &iPosition);
     ...
     if( iEnd<n && z[iEnd]=='*' ) ...
     iEnd 与 iStart不设置正确会出现可怕的结果！会出现越界访问！
     */
    *piStartOffset = wcpos2strpos(c->wcInput, pos);
    
    /* iEndOffset的确定是个非常重要的事情！！！正确处理它非常关键。
       它决定了对查询输入字符串处理过程中nConsumed值的确定，也就决定了查询在何时停止，
       何时输入字符串就已经在exprParse的过程中全部parse完了。
       如果这个值不正确，还有可能出现宽字符的字节在parse过程中被截断等等非常诡异、随机的错误情形。
       针对bigram，它的正确确定应该注意的方面：
       1. 因为bigram方法分词时要overlap，所以每次只步进一字。如"物权法律规定"一词，“物权”->“权法”->“法律”->“律规”->“规定”。
          所以特别注意每次分词实际只consume一个字。那么，会决定nConsumed值计算的iEndOffset必须也符合此点。
          所以：fts3_tokenizer.h文件中的这段说明是不能适用的：
                 ** The input text that generated the token is
                 ** identified by the byte offsets returned in *piStartOffset and
                 ** *piEndOffset. *piStartOffset should be set to the index of the first
                 ** byte of the token in the input buffer. *piEndOffset should be set
                 ** to the index of the first byte just past the end of the token in
                 ** the input buffer.
          这段说明要求iEndOffset指向token之后的第一个字节。而我们这里需要指向该token中第二字的字节！！！
          否则，按照这段说明，每次分词会步进两字，overlap就不能实现了。
          "物权法律规定"一词会分解为“物权”->“法律”->“规定”
       2. 但是，到整个输入短语串都分词完毕后，iEndOffset却必须指向输入短语串的结尾，而不能是按照上面规则所确定的
          最后一词第二字！否则sqlite会认为还未分词完毕，会继续parse。
          在此过程中，还需注意有被略过的字符时，iEndOffset也要正确确定。
     */
    
    if (pos == len-2) { // 已经到最后一词，iEndOffset确定为整个输入字符串的结尾。
        /* 这个处理是不是应该放在返回SQLITE_DONE之前来做？待定。
           不是。
           看fts3_tokenizer.h文件中的说明：
             ** This method should either return SQLITE_OK and set the values of the
             ** "OUT" variables identified below, or SQLITE_DONE to indicate that
             ** the end of the buffer has been reached, or an SQLite error code.
           可见，只有在返回SQLITE_OK（也就代表着这次成功又获取了一个token）时需要对OUT的几个参数的值进行设定。
           返回SQLITE_DONE意味着token已经获取完了，这次也没有获取到token，工作结束。
         */
        *piEndOffset = c->nInput;
    } else { // 还未到最后一词，按一般情况来，iEndOffset确定为token第二字的字节起始位置。
        char tmp[7];
        *piEndOffset = *piStartOffset + utf8_wctomb(tmp, c->wcInput[pos], 6);
    }
    
    
    /* 分词读取的起始位置（在input的宽字符串中）在现pos的基础上加1
       注意：pos在前面略过非成词字符处理时可能已经发生增长，所以这里不能简单的
       c->iOffset_in_wcInput++，而必须设定为pos+1
     */
    c->iOffset_in_wcInput = pos+1;
    
    return SQLITE_OK;
}

static const sqlite3_tokenizer_module bigramTokenizerModule = {
    0,
    bigramCreate,
    bigramDestroy,
    bigramOpen,
    bigramClose,
    bigramNext,
    0
};

/*
 ** Allocate a new bigram tokenizer.  Return a pointer to the new
 ** tokenizer in *ppModule
 */
void sqlite3Fts3BigramTokenizerModule(
  sqlite3_tokenizer_module const **ppModule
){
    *ppModule = &bigramTokenizerModule;
}


#endif /* !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_FTS3) */