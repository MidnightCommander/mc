#include "slinclud.h"
#include <string.h>

#include "slang.h"
#include "_slang.h"

static unsigned char Len_Map[256] =
{
  0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  /* - 31 */
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  /* - 63 */
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  /* - 95 */
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  /* - 127 */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* - 159 */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* - 191 */
  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  /* - 223 */
  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,1,1   /* - 255 */
};

/*
 * Also note that the code positions U+D800 to U+DFFF (UTF-16 surrogates)
 * as well as U+FFFE and U+FFFF must not occur in normal UTF-8 or UCS-4
 * data. UTF-8 decoders should treat them like malformed or overlong
 * sequences for safety reasons.
 */
#define IS_ILLEGAL_UNICODE(w) \
   (((w >= 0xD800) && (w <= 0xDFFF)) || (w == 0xFFFE) || (w == 0xFFFF))

_INLINE_ 
static int is_invalid_or_overlong_utf8 (SLuchar_Type *u, unsigned int len)
{
   unsigned int i;
   unsigned char ch, ch1;

   /* Check for invalid sequences */
   for (i = 1; i < len; i++)
     {
	if ((u[i] & 0xC0) != 0x80)
	  return 1;
     }

   /* Illegal (overlong) sequences */  
   /*           1100000x (10xxxxxx) */
   /*           11100000 100xxxxx (10xxxxxx) */
   /*           11110000 1000xxxx (10xxxxxx 10xxxxxx) */
   /*           11111000 10000xxx (10xxxxxx 10xxxxxx 10xxxxxx) */
   /*           11111100 100000xx (10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx) */
   ch = *u;
   if ((ch == 0xC0) || (ch == 0xC1))
     return 1;

   ch1 = u[1];
   if (((ch1 & ch) == 0x80)
       && ((ch == 0xE0)
	   || (ch == 0xF0)
	   || (ch == 0xF8)
	   || (ch == 0xFC)))
     return 1;

   if (len == 3)
     {
	/* D800 is encoded as 0xED 0xA0 0x80 and DFFF as 0xED 0xBF 0xBF */
	if ((ch == 0xED)
	    && ((ch1 >= 0xA0) && (ch1 <= 0xBF))
	    && (u[2] >= 0x80) && (u[2] <= 0xBF))
	  return 1;
	/* Now FFFE and FFFF */
	if ((ch == 0xEF)
	    && (ch1 == 0xBF)
	    && ((u[2] == 0xBE) || (u[2] == 0xBF)))
	  return 1;
     }
   return 0;
}

/* This function assumes that the necessary checks have been made to ensure
 * a valid UTF-8 encoded character is present. 
 */
_INLINE_
static SLwchar_Type fast_utf8_decode (SLuchar_Type *u, unsigned int len)
{
   static unsigned char masks[7] =
     {
	0, 0, 0x1F, 0xF, 0x7, 0x3, 0x1
     };
   SLuchar_Type *umax;
   SLwchar_Type w;

   w = (*u & masks[len]);
   umax = u + len;
   u++;
   while (u < umax)
     {
	w = (w << 6)| (u[0] & 0x3F);
	u++;
     }
   return w;
}

unsigned char *SLutf8_skip_char (unsigned char *s, unsigned char *smax)
{
   unsigned int len;
   
   if (s >= smax)
     return s;

   len = Len_Map[*s];
   if (len <= 1)
     return s+1;

   if (s + len > smax)
     return s+1;
   
   if (is_invalid_or_overlong_utf8 (s, len))
     return s + 1;
   
   return s + len;
}

SLuchar_Type *SLutf8_skip_chars (SLuchar_Type *s, SLuchar_Type *smax,
				  unsigned int num, unsigned int *dnum,
				  int ignore_combining)
{
   unsigned int n;

   n = 0;
   while ((n < num) && (s < smax))
     {
	unsigned int len = Len_Map[*s];

	if (len <= 1)
	  {
	     n++;
	     s++;
	     continue;
	  }

	if (s + len > smax)
	  {
	     s++;
	     n++;
	     continue;
	  }

	if (is_invalid_or_overlong_utf8 (s, len))
	  {
	     s++;
	     n++;
	     continue;
	  }
	
	if (ignore_combining)
	  {
	     SLwchar_Type w = fast_utf8_decode (s, len);
	     if (0 != SLwchar_wcwidth (w))
	       n++;
	     s += len;
	     continue;
	  }

	n++;
	s += len;
     }

   if (ignore_combining)
     {
	while (s < smax)
	  {
	     SLwchar_Type w;
	     unsigned int nconsumed;
	     if (NULL == SLutf8_decode (s, smax, &w, &nconsumed))
	       break;
	     
	     if (0 != SLwchar_wcwidth (w))
	       break;
	     
	     s += nconsumed;
	  }
     }

   if (dnum != NULL)
     *dnum = n;
   return s;
}


SLuchar_Type *SLutf8_bskip_chars (SLuchar_Type *smin, SLuchar_Type *s,
				   unsigned int num, unsigned int *dnum,
				   int ignore_combining)
{
   unsigned int n;
   SLuchar_Type *smax = s;

   n = 0;
   while ((n < num) && (s > smin))
     {
	unsigned char ch;
	unsigned int dn;

	s--;
	ch = *s;
	if (ch < 0x80)
	  {
	     n++;
	     smax = s;
	     continue;
	  }
	
	dn = 0;
	while ((s != smin) 
	       && (Len_Map[ch] == 0)
	       && (dn < SLUTF8_MAX_MBLEN))
	  {
	     s--;
	     ch = *s;
	     dn++;
	  }

	if (ch <= 0xBF)
	  {
	     /* Invalid sequence */
	     n++;
	     smax--;
	     s = smax;
	     continue;
	  }

	if (ch > 0xBF)
	  {
	     SLwchar_Type w;
	     SLuchar_Type *s1;

	     if ((NULL == (s1 = SLutf8_decode (s, smax, &w, NULL)))
		 || (s1 != smax))
	       {
		  /* This means we backed up over an invalid sequence */
		  dn = (unsigned int) (smax - s);
		  n++;
		  smax--;
		  s = smax;
		  continue;
	       }
	     
	     if ((ignore_combining == 0) 
		 || (0 != SLwchar_wcwidth (w)))
	       n++;

	     smax = s;
	  }
     }

   if (dnum != NULL)
     *dnum = n;
   return s;
}

SLuchar_Type *SLutf8_bskip_char (SLuchar_Type *smin, SLuchar_Type *s)
{
   if (s > smin)
     {
	unsigned int dn;

	s--;
	if (*s >= 0x80)
	  s = SLutf8_bskip_chars (smin, s+1, 1, &dn, 0);
     }
   return s;
}


/* This function counts the number of wide characters in a UTF-8 encoded
 * string.  Each byte in an invalid sequence is counted as a single character.
 * If the string contains illegal values, the bytes making up the character is
 * counted as 1 character.
 */
unsigned int SLutf8_strlen (SLuchar_Type *s, int ignore_combining)
{
   unsigned int count, len;
   
   if (s == NULL)
     return 0;

   len = strlen ((char *)s);
   (void) SLutf8_skip_chars (s, s + len, len, &count, ignore_combining);
   return count;
}


/*
 * This function returns NULL if the input does not correspond to a valid
 * UTF-8 sequence, otherwise, it returns the position of the next character
 * in the sequence.
 */
unsigned char *SLutf8_decode (unsigned char *u, unsigned char *umax,
			      SLwchar_Type *wp, unsigned int *nconsumedp)
{
   unsigned int len;
   unsigned char ch;
   SLwchar_Type w;

   if (u >= umax)
     {
	*wp = 0;
	if (nconsumedp != NULL)
	  *nconsumedp = 0;
	return NULL;
     }

   *wp = ch = *u;
   if (ch < 0x80)
     {
	if (nconsumedp != NULL) *nconsumedp = 1;
	return u+1;
     }
   
   len = Len_Map[ch];
   if (len < 2)
     {
	/* should not happen--- code here for completeness */
	if (nconsumedp != NULL) *nconsumedp = 1;
	return NULL;
     }
   if (u + len > umax)
     {
	if (nconsumedp != NULL) *nconsumedp = 1; /* (unsigned int) (umax - u); */
	return NULL;
     }

   if (is_invalid_or_overlong_utf8 (u, len))
     {
	if (nconsumedp != NULL)
	  *nconsumedp = 1;
	
	return NULL;
     }

   if (nconsumedp != NULL) 
     *nconsumedp = len;

   *wp = w = fast_utf8_decode (u, len);

   if (IS_ILLEGAL_UNICODE(w))
     return NULL;

   return u + len;
}


/* Encode the wide character returning a pointer to the end of the 
 * utf8 of the encoded multi-byte character.  This function will also encode
 * illegal unicode values.  It returns NULL if buflen is too small. 
 * Otherwise, it returns a pointer at the end of the last encoded byte.
 * It does not null terminate the encoded string.
 */
SLuchar_Type *SLutf8_encode (SLwchar_Type w, SLuchar_Type *u, unsigned int ulen)
{
   SLuchar_Type *umax = u + ulen;
   
   /*   U-00000000 - U-0000007F: 0xxxxxxx */
   if (w <= 0x7F)
     {
	if (u >= umax)
	  return NULL;

	*u++ = (unsigned char) w;
	return u;
     }

   /*   U-00000080 - U-000007FF: 110xxxxx 10xxxxxx */
   if (w <= 0x7FF)
     {
	if ((u + 1) >= umax)
	  return NULL;

	*u++ = (w >> 6) | 0xC0;
	*u++ = (w & 0x3F) | 0x80;
	return u;
     }

   /* First bad character starts at 0xD800 */

   /* Allow illegal values to be encoded */

   /*
    *if (IS_ILLEGAL_UNICODE(w))
    * return NULL;
    */

   /*   U-00000800 - U-0000FFFF: 1110xxxx 10xxxxxx 10xxxxxx */
   if (w <= 0xFFFF)
     {
	if (u+2 >= umax) 
	  return NULL;
	*u++ = (w >> 12 ) | 0xE0;
	goto finish_2;
     }
   
   /*   U-00010000 - U-001FFFFF: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
   if (w <= 0x1FFFFF)
     {
	if (u+3 >= umax)
	  return NULL;
	*u++ = (w >> 18) | 0xF0;
	goto finish_3;
     }

   /*   U-00200000 - U-03FFFFFF: 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx */
   if (w <= 0x3FFFFFF)
     {
	if (u+4 >= umax)
	  return NULL;
	*u++ = (w >> 24) | 0xF8;
	goto finish_4;
     }
   
   /*   U-04000000 - U-7FFFFFFF: 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx */
   if (w <= 0x7FFFFFFF)
     {
	if (u+5 >= umax)
	  return NULL;
	*u++ = (w >> 30) | 0xFC;
	goto finish_5;
     }

   /* unreached?? */
   return NULL;

   finish_5: *u++ = ((w >> 24) & 0x3F)|0x80;
   finish_4: *u++ = ((w >> 18) & 0x3F)|0x80;
   finish_3: *u++ = ((w >> 12) & 0x3F)|0x80;
   finish_2: *u++ = ((w >> 6) & 0x3F)|0x80;
             *u++ = (w & 0x3F)|0x80;
   
   return u;
}

/* Like SLutf8_encode, but null terminates the result.  
 * At least SLUTF8_MAX_MBLEN+1 bytes assumed.
 */
SLuchar_Type *SLutf8_encode_null_terminate (SLwchar_Type w, SLuchar_Type *u)
{
   SLuchar_Type *p;
   
   p = SLutf8_encode (w, u, SLUTF8_MAX_MBLEN);
   if (p != NULL)
     *p = 0;
   return p;
}

#if 0
int SLutf8_decode_bytes (SLuchar_Type *u, SLuchar_Type *umax,
			 unsigned char *b, unsigned int *np)
{
   unsigned char *bmax;

   bmax = b;
   while (u < umax)
     {
	SLwchar_Type w;

	if (0 == (*u & 0x80))
	  {
	     *bmax++ = *u++;
	     continue;
	  }
	
	if (NULL == (u = SLutf8_decode (u, umax, &w, NULL)))
	  return -1;		       /* FIXME: HANDLE ERROR */
	
	if (w > 0xFF)
	  { 
#if 0
	     sprintf (bmax, "<U+%04X>", w);
	     bmax += strlen (bmax);
	     continue;
#endif
	     /* FIXME: HANDLE ERROR */
	     w = w & 0xFF;
	  }
	
	*bmax++ = w;
     }
   *np = bmax - b;
   *bmax = 0;
   return 0;
}

/* UTF-8 Encode the bytes between b and bmax storing the results in the
 * buffer defined by u and umax, returning the position following the 
 * last encoded character.  Upon return, *np is set to the number of bytes
 * sucessfully encoded.
 */
SLuchar_Type *SLutf8_encode_bytes (unsigned char *b, unsigned char *bmax,
				   SLuchar_Type *u, unsigned int ulen, 
				   unsigned int *np)
{
   unsigned char *bstart = b;
   SLuchar_Type *umax = u + ulen;

   while (b < bmax)
     {
	SLuchar_Type *u1;

	if (0 == (*b & 0x80))
	  {
	     if (u >= umax)
	       break;
	     
	     *u++ = *b++;
	     continue;
	  }

	if (NULL == (u1 = SLutf8_encode (*b, u, umax - u)))
	  break;
	u = u1;
	b++;
     }

   *np = b - bstart;
   if (u < umax)
     *u = 0;

   return u;
}
#endif

static SLuchar_Type *xform_utf8 (SLuchar_Type *u, SLuchar_Type *umax,
                                 SLwchar_Type (*fun)(SLwchar_Type))
{
   SLuchar_Type *buf, *p;
   unsigned int malloced_len, len;

   if (umax < u)
     return NULL;
   
   len = 0;
   p = buf = NULL;
   malloced_len = 0;

   while (1)
     {
        SLwchar_Type w;
        SLuchar_Type *u1;
        unsigned int nconsumed;

        if (malloced_len <= len + SLUTF8_MAX_MBLEN)
          {
             SLuchar_Type *newbuf;
             malloced_len += 1 + (umax - u) + SLUTF8_MAX_MBLEN;
             
             newbuf = (SLuchar_Type *)SLrealloc ((char *)buf, malloced_len);
             if (newbuf == NULL)
               {
                  SLfree ((char *)buf);
                  return NULL;
               }
             buf = newbuf;
             p = buf + len;
          }

        if (u >= umax)
          {
             *p = 0;
             p = (SLuchar_Type *) SLang_create_nslstring ((char *)buf, len);
             SLfree ((char *)buf);
             return p;
          }

        if (NULL == (u1 = SLutf8_decode (u, umax, &w, &nconsumed)))
          {
             /* Invalid sequence */
             memcpy ((char *) p, u, nconsumed);
             p += nconsumed;
             len += nconsumed;
             u1 = u + nconsumed;
          }
        else
          {
             SLuchar_Type *p1;

             p1 = SLutf8_encode ((*fun)(w), p, malloced_len);
             if (p1 == NULL)
               {
                  SLfree ((char *)buf);
                  SLang_verror (SL_INTERNAL_ERROR, "SLutf8_encode returned NULL");
                  return NULL;
               }
             len += p1 - p;
             p = p1;
          }
        
        u = u1;
     }
}

/* Returned an uppercased version of an UTF-8 encoded string.  Illegal or
 * invalid sequences will be returned as-is.  This function returns 
 * an SLstring.
 */
SLuchar_Type *SLutf8_strup (SLuchar_Type *u, SLuchar_Type *umax)
{
   return xform_utf8 (u, umax, SLwchar_toupper);
}

/* Returned an lowercased version of an UTF-8 encoded string.  Illegal or
 * invalid sequences will be returned as-is.  This function returns 
 * an SLstring.
 */
SLuchar_Type *SLutf8_strlo (SLuchar_Type *u, SLuchar_Type *umax)
{
   return xform_utf8 (u, umax, SLwchar_tolower);
}

int SLutf8_compare (SLuchar_Type *a, SLuchar_Type *amax, 
                    SLuchar_Type *b, SLuchar_Type *bmax,
                    unsigned int nchars,
                    int cs)
{
   while (nchars && (a < amax) && (b < bmax))
     {
        SLwchar_Type cha, chb;
        unsigned int na, nb;
        int aok, bok;

        if (*a < 0x80)
          {
             cha = (SLwchar_Type) *a++;
             aok = 1;
          }
        else
          {
             aok = (NULL != SLutf8_decode (a, amax, &cha, &na));
             a += na;
          }

        if (*b < 0x80)
          {
             chb = (SLwchar_Type) *b++;
             bok = 1;
          }
        else
          {
             bok = (NULL != SLutf8_decode (b, bmax, &chb, &nb));
             b += nb;
          }

        nchars--;

        if (aok && bok)
          {
             if (cs == 0)
               {
                  cha = SLwchar_toupper (cha);
                  chb = SLwchar_toupper (chb);
               }
          }
        else if (aok)
          return 1;
        else if (bok)
          return -1;
        
        if (cha == chb)
          continue;
        
        if (cha > chb)
          return 1;

        return -1;
     }

   if (nchars == 0)
     return 0;

   if ((a >= amax) && (b >= bmax))
     return 0;
   
   if (b >= bmax)
     return 1;
   
   return -1;
}


/* Returns an SLstring */
SLstr_Type *SLutf8_subst_wchar (SLuchar_Type *u, SLuchar_Type *umax,
				SLwchar_Type wch, unsigned int pos,
				int ignore_combining)
{
   SLuchar_Type *a, *a1, *b;
   unsigned int dpos;
   SLuchar_Type buf[SLUTF8_MAX_MBLEN+1];
   SLstr_Type *c;
   unsigned int n1, n2, n3, len;

   a = SLutf8_skip_chars (u, umax, pos, &dpos, ignore_combining);

   if ((dpos != pos) || (a == umax))
     {
	SLang_verror (SL_INDEX_ERROR, "Specified character position is invalid for string");
	return NULL;
     }

   a1 = SLutf8_skip_chars (a, umax, 1, NULL, ignore_combining);
   
   b = SLutf8_encode (wch, buf, SLUTF8_MAX_MBLEN);
   if (b == NULL)
     {
	SLang_verror (SL_UNICODE_ERROR, "Unable to encode wchar 0x%lX", (unsigned long)wch);
	return NULL;
     }
   
   n1 = (a-u);
   n2 = (b-buf);
   n3 = (umax-a1);
   len = n1 + n2 + n3;
   c = _pSLallocate_slstring (len);
   if (c == NULL)
     return NULL;
   
   memcpy (c, (char *)u, n1);
   memcpy (c+n1, (char *)buf, n2);
   memcpy (c+n1+n2, (char *)a1, n3);
   c[len] = 0;

   /* No need to worry about this failing-- it frees its argument */
   return _pSLcreate_via_alloced_slstring (c, len);
}

   
/* utf8 buffer assumed to be at least SLUTF8_MAX_MBLEN+1 bytes.  Result will be
 * null terminated.   Returns position of NEXT character.
 * Analogous to: *p++
 */
SLuchar_Type *SLutf8_extract_utf8_char (SLuchar_Type *u,
					SLuchar_Type *umax, 
					SLuchar_Type *utf8)
{
   SLuchar_Type *u1;
   
   u1 = SLutf8_skip_char (u, umax);
   memcpy ((char *)utf8, u, u1-u);
   utf8[u1-u] = 0;
   
   return u1;
}
   
   

/* These routines depend upon the value of the _pSLinterp_UTF8_Mode variable.
 * They also generate slang errors upon error.
 */
SLuchar_Type *_pSLinterp_decode_wchar (SLuchar_Type *u, 
				      SLuchar_Type *umax, 
				      SLwchar_Type *chp)
{
   if (_pSLinterp_UTF8_Mode == 0)
     {
	if (u < umax)
	  *chp = (SLwchar_Type) *u++;
	return u;
     }

   if (NULL == (u = SLutf8_decode (u, umax, chp, NULL)))
     SLang_verror (SL_INVALID_UTF8, "Invalid UTF-8 encoded string");
   
   return u;
}

/* At least SLUTF8_MAX_MBLEN+1 bytes assumed-- null terminates result.
 * Upon success, it returns a pointer to the _end_ of the encoded character
 */
SLuchar_Type *_pSLinterp_encode_wchar (SLwchar_Type wch, SLuchar_Type *u, unsigned int *encoded_len)
{
   SLuchar_Type *u1;

   if (_pSLinterp_UTF8_Mode == 0)
     {
	*encoded_len = 1;
	*u++ = (SLuchar_Type) wch;
	*u++ = 0;
	return u;
     }

   if (NULL == (u1 = SLutf8_encode_null_terminate (wch, u)))
     {
	SLang_verror (SL_UNICODE_ERROR, "Unable to encode character 0x%lX", (unsigned long)wch);
	return NULL;
     }

   *encoded_len = (unsigned int) (u1 - u);
   return u1;
}

#ifdef REGRESSION
int main (int argc, char **argv)
{
   unsigned char *s, *smax;
   char **t;
   char *ok_tests [] = 
     {
	"íŸ¿",
	  "î€€",
	  "ï¿½",
	  "ô¿¿",
	  "ô€€",
	  NULL
     };
   char *long_tests [] = 
     {
	"À¯",
	  "à€¯",
	  "ð€€¯",
	  "ø€€€¯",
	  "ü€€€€¯",
	  NULL
     };

   t = long_tests;
   while ((s = (unsigned char *) *t++) != NULL)
     {
	smax = s + strlen ((char *)s);

	while (s < smax)
	  {
	     SLwchar_Type w;
	     
	     if (NULL == (s = SLutf8_to_wc (s, smax, &w)))
	       {
		  fprintf (stderr, "SLutf8_to_wc failed\n");
		  break;
	       }
	     if (w == 0)
	       break;
	     fprintf (stdout, " 0x%X", w);
	  }
   
	fprintf (stdout, "\n");
     }
   return 0;
}
#endif

