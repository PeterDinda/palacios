/* ezxml.c
 *
 * Copyright 2004-2006 Aaron Voisine <aaron@voisine.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* 
 * Modified for Palacios by Jack Lange <jarusl@cs.northwestern.edu> 
 */



#include <palacios/vmm_xml.h>
#include <palacios/vmm_sprintf.h>
#include <stdarg.h>
#include <palacios/vmm.h>

#define V3_XML_BUFSIZE 1024 // size of internal memory buffers

// Flags for struct v3_xml 
#define V3_XML_NAMEM   0x80 // name is malloced
#define V3_XML_TXTM    0x40 // txt is malloced
#define V3_XML_DUP     0x20 // attribute name and value are strduped
//

static char * V3_XML_NIL[] = { NULL }; // empty, null terminated array of strings


#define V3_XML_WS   "\t\r\n "  // whitespace
#define V3_XML_ERRL 128        // maximum error string length

struct v3_xml_root {       // additional data for the root tag
    struct v3_xml xml;     // is a super-struct built on top of v3_xml struct
    struct v3_xml * cur;          // current xml tree insertion point
    char *str_ptr;         // original xml string
    char *tmp_start;              // start of work area
    char *tmp_end;              // end of work area
    short standalone;     // non-zero if <?xml standalone="yes"?>
    char err[V3_XML_ERRL]; // error string
};

static char * empty_attrib_list[] = { NULL }; // empty, null terminated array of strings



static void * tmp_realloc(void * old_ptr, size_t old_size, size_t new_size) {
    void * new_buf = NULL; 

    new_buf = V3_Malloc(new_size);
    
    if (new_buf == NULL) {
        return NULL;
    }

    memset(new_buf, 0, new_size);

    memcpy(new_buf, old_ptr, old_size);
    V3_Free(old_ptr);

    return new_buf;
}

// set an error string and return root
static void v3_xml_err(struct v3_xml_root * root, char * xml_str, const char * err, ...) {
    va_list ap;
    int line = 1;
    char * tmp;
    char fmt[V3_XML_ERRL];
    
    for (tmp = root->tmp_start; tmp < xml_str; tmp++) {
	if (*tmp == '\n') {
	    line++;
	}
    }

    snprintf(fmt, V3_XML_ERRL, "[error near line %d]: %s", line, err);

    va_start(ap, err);
    vsnprintf(root->err, V3_XML_ERRL, fmt, ap);
    va_end(ap);

    PrintError("XML Error: %s\n", root->err);

    // free memory
    v3_xml_free(&(root->xml));

    return;
}



// returns the first child tag with the given name or NULL if not found
struct v3_xml * v3_xml_child(struct v3_xml * xml, const char * name) {
    struct v3_xml * child = NULL;

    if (xml != NULL) {
	child = xml->child;
    }

    while ((child) && (strcasecmp(name, child->name) != 0)) {
	child = child->sibling;
    }

    return child;
}

// returns the Nth tag with the same name in the same subsection or NULL if not
// found
struct v3_xml * v3_xml_idx(struct v3_xml * xml, int idx) {
    for (; xml && idx; idx--) {
	xml = xml->next;
    }

    return xml;
}

// returns the value of the requested tag attribute or NULL if not found
const char * v3_xml_attr(struct v3_xml * xml, const char * attr) {
    int i = 0;

    if ((!xml) || (!xml->attr)) {
	return NULL;
    }

    while ((xml->attr[i]) && (strcasecmp(attr, xml->attr[i]) != 0)) {
	i += 2;
    }

    if (xml->attr[i] != NULL) {
	return xml->attr[i + 1]; // found attribute
    }

    return NULL; // found default
}

// same as v3_xml_get but takes an already initialized va_list
static struct v3_xml * v3_xml_vget(struct v3_xml * xml, va_list ap) {
    char * name = va_arg(ap, char *);
    int idx = -1;

    if ((name != NULL) && (*name != 0)) {
        idx = va_arg(ap, int);    
        xml = v3_xml_child(xml, name);
    }
    return (idx < 0) ? xml : v3_xml_vget(v3_xml_idx(xml, idx), ap);
}

// Traverses the xml tree to retrieve a specific subtag. Takes a variable
// length list of tag names and indexes. The argument list must be terminated
// by either an index of -1 or an empty string tag name. Example: 
// title = v3_xml_get(library, "shelf", 0, "book", 2, "title", -1);
// This retrieves the title of the 3rd book on the 1st shelf of library.
// Returns NULL if not found.
struct v3_xml * v3_xml_get(struct v3_xml * xml, ...) {
    va_list ap;
    struct v3_xml * r;

    va_start(ap, xml);
    r = v3_xml_vget(xml, ap);
    va_end(ap);
    return r;
}


// sets a flag for the given tag and returns the tag
static struct v3_xml * v3_xml_set_flag(struct v3_xml * xml, short flag)
{
    if (xml) {
	xml->flags |= flag;
    }
    return xml;
}





// Recursively decodes entity and character references and normalizes new lines
// ent is a null terminated array of alternating entity names and values. set t
// to '&' for general entity decoding, '%' for parameter entity decoding, 'c'
// for cdata sections, ' ' for attribute normalization, or '*' for non-cdata
// attribute normalization. Returns s, or if the decoded string is longer than
// s, returns a malloced string that must be freed.
static char * v3_xml_decode(char * s, char t) {
    char * e;
    char * r = s;
    long c, l;

    // normalize line endings
    for (; *s; s++) { 
        while (*s == '\r') {
            *(s++) = '\n';

            if (*s == '\n') {
		memmove(s, (s + 1), strlen(s));
	    }
        }
    }
    
    for (s = r; ; ) {

        while ( (*s) && 
		(*s != '&') && 
		( (*s != '%') || 
		  (t != '%')) && 
		(!isspace(*s))) {
	    s++;
	}

        if (*s == '\0') {
	    break;
	}

	if ((t != 'c') && (strncmp(s, "&#", 2) == 0)) { // character reference
            if (s[2] == 'x') {
		c = strtox(s + 3, &e); // base 16
	    } else {
		c = strtoi(s + 2, &e); // base 10
	    }

            if ((!c) || (*e != ';')) { 
		// not a character ref
		s++; 
		continue;
	    }

	    *(s++) = c; 

            memmove(s, strchr(s, ';') + 1, strlen(strchr(s, ';')));
        } else if ( ( (t == ' ') || (t == '*')) && 
		    (isspace(*s))) {
	    *(s++) = ' ';
	} else {
	    // no decoding needed
	    s++;
	}
    }

    if (t == '*') { 
	// normalize spaces for non-cdata attributes
        for (s = r; *s; s++) {
            if ((l = strspn(s, " "))) {
		memmove(s, s + l, strlen(s + l) + 1);
	    }

            while ((*s) && (*s != ' ')) {
		s++;
	    }
        }

        if ((--s >= r) && (*s == ' ')) {
	    // trim any trailing space
	    *s = '\0'; 
	}
    }
    return r;
}



// called when parser finds character content between open and closing tag
static void v3_xml_char_content(struct v3_xml_root * root, char * s, size_t len, char t) {
    struct v3_xml * xml = root->cur;
    char * m = s;
    size_t l = 0;

    if ((xml == NULL) || (xml->name == NULL) || (len == 0)) {
	// sanity check
	return;
    }

    s[len] = '\0'; // null terminate text (calling functions anticipate this)
    len = strlen(s = v3_xml_decode(s, t)) + 1;

    if (xml->txt[0] == '\0') { // empty string
	// initial character content
	xml->txt = s;
    } else { 

	// allocate our own memory and make a copy
	if (xml->flags & V3_XML_TXTM) {
	    xml->txt = (tmp_realloc(xml->txt, strlen(xml->txt), (l = strlen(xml->txt)) + len));
	} else {
	    char * tmp = NULL;

	    tmp = V3_Malloc((l = strlen(xml->txt)) + len);
	    strcpy(tmp, xml->txt);
	    xml->txt = tmp;
	}

        strcpy(xml->txt + l, s); // add new char content
	
        if (s != m) {
	    V3_Free(s); // free s if it was malloced by v3_xml_decode()
	}
    }

    if (xml->txt != m) {
	v3_xml_set_flag(xml, V3_XML_TXTM);
    }
}

// called when parser finds closing tag
static int v3_xml_close_tag(struct v3_xml_root * root, char * name, char * s) {
    if ( (root->cur == NULL) || 
	 (root->cur->name == NULL) || 
	 (strcasecmp(name, root->cur->name))) {
	v3_xml_err(root, s, "unexpected closing tag </%s>", name);
	return -1;
    }

    root->cur = root->cur->parent;
    return 0;
}


// frees a tag attribute list
static void v3_xml_free_attr(char **attr) {
    int i = 0;
    char * m;
    
    if ((attr == NULL) || (attr == empty_attrib_list)) {
	// nothing to free
	return; 
    }

    while (attr[i]) {
	// find end of attribute list
	i += 2;
    }

    m = attr[i + 1]; // list of which names and values are malloced

    V3_Free(m);
    V3_Free(attr);
}






// returns a new empty v3_xml structure with the given root tag name
static struct v3_xml * v3_xml_new(const char * name) {

    struct v3_xml_root * root = (struct v3_xml_root *)V3_Malloc(sizeof(struct v3_xml_root));
    memset(root, 0, sizeof(struct v3_xml_root));

    root->xml.name = (char *)name;
    root->cur = &root->xml;
    root->xml.txt = "";
    memset(root->err, 0, V3_XML_ERRL);


    return &root->xml;
}

// inserts an existing tag into an v3_xml structure
struct v3_xml * v3_xml_insert(struct v3_xml * xml, struct v3_xml * dest, size_t off) {
    struct v3_xml * cur, * prev, * head;

    xml->next = NULL;
    xml->sibling = NULL;
    xml->ordered = NULL;
    
    xml->off = off;
    xml->parent = dest;

    if ((head = dest->child)) { 
	// already have sub tags

        if (head->off <= off) {
	    // not first subtag

            for (cur = head; 
		 ((cur->ordered) && (cur->ordered->off <= off));
                 cur = cur->ordered);

            xml->ordered = cur->ordered;
            cur->ordered = xml;
        } else { 
	    // first subtag
            xml->ordered = head;
            dest->child = xml;
        }

	// find tag type
        for (cur = head, prev = NULL; 
	     ((cur) && (strcasecmp(cur->name, xml->name) != 0));
             prev = cur, cur = cur->sibling); 


        if (cur && cur->off <= off) { 
	    // not first of type
            
	    while (cur->next && cur->next->off <= off) {
		cur = cur->next;
	    }

            xml->next = cur->next;
            cur->next = xml;
        } else { 
	    // first tag of this type
	    
            if (prev && cur) {
		// remove old first
		prev->sibling = cur->sibling;
	    }

            xml->next = cur; // old first tag is now next

	    // new sibling insert point
            for (cur = head, prev = NULL; 
		 ((cur) && (cur->off <= off));
                 prev = cur, cur = cur->sibling);
	    
            xml->sibling = cur;

            if (prev) {
		prev->sibling = xml;
	    }
        }
    } else {
	// only sub tag
	dest->child = xml;
    }

    return xml;
}


// Adds a child tag. off is the offset of the child tag relative to the start
// of the parent tag's character content. Returns the child tag.
static struct v3_xml * v3_xml_add_child(struct v3_xml * xml, const char * name, size_t off) {
    struct v3_xml * child;

    if (xml == NULL) {
	return NULL;
    }

    child = (struct v3_xml *)V3_Malloc(sizeof(struct v3_xml));
    memset(child, 0, sizeof(struct v3_xml));

    child->name = (char *)name;
    child->attr = empty_attrib_list;
    child->txt = "";

    return v3_xml_insert(child, xml, off);
}


// called when parser finds start of new tag
static void v3_xml_open_tag(struct v3_xml_root * root, char * name, char ** attr) {
    struct v3_xml * xml = root->cur;
    
    if (xml->name) {
	xml = v3_xml_add_child(xml, name, strlen(xml->txt));
    } else {
	// first open tag
	xml->name = name; 
    }

    xml->attr = attr;
    root->cur = xml; // update tag insertion point
}







// parse the given xml string and return an v3_xml structure
static struct v3_xml * parse_str(char * buf, size_t len) {
    struct v3_xml_root * root = (struct v3_xml_root *)v3_xml_new(NULL);
    char quote_char;
    char last_char; 
    char * tag_ptr;
    char ** attr; 
    int attr_idx;

    root->str_ptr = buf;

    if (len == 0) {
	v3_xml_err(root, NULL, "Empty XML String\n");
	return NULL;
    }

    root->tmp_start = buf;
    root->tmp_end = buf + len; // record start and end of work area
    
    last_char = buf[len - 1]; // save end char
    buf[len - 1] = '\0'; // turn end char into null terminator

    while ((*buf) && (*buf != '<')) {
	// find first tag
	buf++; 
    }

    if (*buf == '\0') {
	v3_xml_err(root, buf, "root tag missing");
	return NULL;
    }

    for (; ; ) {
        attr = (char **)empty_attrib_list;
        tag_ptr = ++buf; // skip first '<'
        
        if (isalpha(*buf) || (*buf == '_') || (*buf == ':') || (*buf < '\0')) {
	    // new tag

            if (root->cur == NULL) {
                v3_xml_err(root, tag_ptr, "markup outside of root element");
		return NULL;
	    }

            buf += strcspn(buf, V3_XML_WS "/>");

            while (isspace(*buf)) {
		// null terminate tag name, 
		// this writes '\0' to spaces after first tag 
		*(buf++) = '\0';
	    }

	

	    // attributes are name value pairs, 
	    //     2nd to last entry is null  (end of list)
	    //     last entry points to a string map marking whether values have been malloced...
	    // loop through attributes until hitting the closing bracket
            for (attr_idx = 0; 
		 (*buf) && (*buf != '/') && (*buf != '>'); 
		 attr_idx += 2) {
		// buf is incremented later on
		// new attrib
		int attr_cnt = (attr_idx / 2) + 1;
		int val_idx = attr_idx + 1;
		int term_idx = attr_idx + 2;
		int last_idx = attr_idx + 3;

		// attr = allocated space
		// attr[val_idx] = mem for list of maloced vals
		if (attr_cnt > 1) {
		    attr = tmp_realloc(attr,
				       (((attr_cnt - 1) * (2 * sizeof(char *))) + 
					(2 * sizeof(char *))), 
				       ((attr_cnt * (2 * sizeof(char *))) + 
					(2 * sizeof(char *))));

		    attr[last_idx] = tmp_realloc(attr[last_idx - 2], 
						 attr_cnt,
						 (attr_cnt + 1)); 
		} else {
		    attr = V3_Malloc(4 * sizeof(char *)); 
		    attr[last_idx] = V3_Malloc(2);
		}

                attr[attr_idx] = buf; // set attribute name
                attr[val_idx] = ""; // temporary attribute value
                attr[term_idx] = NULL; // null terminate list
                strcpy(attr[last_idx] + attr_cnt, " "); // value is not malloc'd, offset into the stringmap

                buf += strcspn(buf, V3_XML_WS "=/>");

                if ((*buf == '=') || isspace(*buf)) {

		    *(buf++) = '\0'; // null terminate tag attribute name

		    // eat whitespace (and more multiple '=' ?)
		    buf += strspn(buf, V3_XML_WS "=");

		    quote_char = *buf;

		    if ((quote_char == '"') || (quote_char == '\'')) { // attribute value
                        attr[val_idx] = ++buf;

                        while ((*buf) && (*buf != quote_char)) {
			    buf++;
			}

                        if (*buf) {
			    // null terminate attribute val
			    *(buf++) = '\0';
			} else {
                            v3_xml_free_attr(attr);
                            v3_xml_err(root, tag_ptr, "missing %c", quote_char);
			    return NULL;
                        }

                        attr[val_idx] = v3_xml_decode(attr[val_idx], ' ');
                    }
                }

                while (isspace(*buf)) {
		    buf++;
		}
            }

            if (*buf == '/') { 
		// self closing tag
                *(buf++) = '\0';
                
		if ( ((*buf) && (*buf != '>')) || 
		     ((!*buf) && (last_char != '>'))) {

                    if (attr_idx > 0) {
			v3_xml_free_attr(attr);
		    }
		    v3_xml_err(root, tag_ptr, "missing >");
		    return NULL;
                }
                v3_xml_open_tag(root, tag_ptr, attr);
                v3_xml_close_tag(root, tag_ptr, buf);
            } else if (((quote_char = *buf) == '>') || 
		       ((!*buf) && (last_char == '>'))) {
		// open tag
                *buf = '\0'; // temporarily null terminate tag name
                v3_xml_open_tag(root, tag_ptr, attr);
                *buf = quote_char;
            } else {
                if (attr_idx > 0) {
		    v3_xml_free_attr(attr);
		}
		v3_xml_err(root, tag_ptr, "missing >"); 
		return NULL;
            }
        } else if (*buf == '/') { 
	    // close tag
            
	    buf += strcspn(tag_ptr = buf + 1, V3_XML_WS ">") + 1;
            
	    quote_char = *buf;
	    if ((*buf == '\0') && (last_char != '>')) {
		v3_xml_err(root, tag_ptr, "missing >");
		return NULL;
            }

	    *buf = '\0'; // temporarily null terminate tag name
            
	    if (v3_xml_close_tag(root, tag_ptr, buf) == -1) {
		return NULL;
	    }

	    *buf = quote_char;
            if (isspace(*buf)) {
		buf += strspn(buf, V3_XML_WS);
	    }
        } else if (strncmp(buf, "!--", 3) == 0) {
	    // xml comment
            if ( ((buf = strstr(buf + 3, "--")) == 0) || 
		 ((*(buf += 2) != '>') && (*buf)) ||
		 ((!*buf) && (last_char != '>'))) {
		v3_xml_err(root, tag_ptr, "unclosed <!--");
		return NULL;
	    }
        } else if (! strncmp(buf, "![CDATA[", 8)) { 
	    // cdata
            if ((buf = strstr(buf, "]]>"))) {
                v3_xml_char_content(root, tag_ptr + 8, (buf += 2) - tag_ptr - 10, 'c');
	    } else {
		v3_xml_err(root, tag_ptr, "unclosed <![CDATA[");
		return NULL;
	    }
	} else {
	    v3_xml_err(root, tag_ptr, "unexpected <");
	    return NULL;
        }

        if (! buf || ! *buf) {
	    break;
	}

        *buf = '\0';
        tag_ptr = ++buf;

	/* Eat leading whitespace */
	while (*buf && isspace(*buf)) {
	    buf++;
	}

        if (*buf && (*buf != '<')) { 
	    // tag character content
            while (*buf && (*buf != '<')) {
		buf++;
	    }

            if (*buf) { 
		v3_xml_char_content(root, tag_ptr, buf - tag_ptr, '&');
	    } else {
		break;
	    }
        } else if (*buf == '\0') {
	    break;
	}
    }

    if (root->cur == NULL) {
	return &root->xml;
    } else if (root->cur->name == NULL) {
	v3_xml_err(root, tag_ptr, "root tag missing");
	return NULL;
    } else {
	v3_xml_err(root, tag_ptr, "unclosed tag <%s>", root->cur->name);
	return NULL;
    }
}


struct v3_xml * v3_xml_parse(char * buf) {
    int str_len = 0;
    char * xml_buf = NULL;

    if (!buf) {
	return NULL;
    }

    str_len = strlen(buf);
    xml_buf = (char *)V3_Malloc(str_len + 1);
    strcpy(xml_buf, buf);

    return parse_str(xml_buf, str_len);
}



// free the memory allocated for the v3_xml structure
void v3_xml_free(struct v3_xml * xml) {
    struct v3_xml_root * root = (struct v3_xml_root *)xml;

    if (xml == NULL) {
        return;
    }

    v3_xml_free(xml->child);
    v3_xml_free(xml->ordered);

    if (xml->parent == NULL) { 
	// free root tag allocations
	V3_Free(root->str_ptr); // malloced xml data
    }

    v3_xml_free_attr(xml->attr); // tag attributes



    if ((xml->flags & V3_XML_TXTM)) {
	// character content
	V3_Free(xml->txt); 
    }

    if ((xml->flags & V3_XML_NAMEM)) {
	// tag name
	V3_Free(xml->name);
    }

    V3_Free(xml);
}





/* Adding XML data */




// sets the character content for the given tag and returns the tag
struct v3_xml *  v3_xml_set_txt(struct v3_xml * xml, const char *txt) {
    if (! xml) {
	return NULL;
    }

    if (xml->flags & V3_XML_TXTM) {
	// existing txt was malloced
	V3_Free(xml->txt); 
    }

    xml->flags &= ~V3_XML_TXTM;
    xml->txt = (char *)txt;
    return xml;
}

// Sets the given tag attribute or adds a new attribute if not found. A value
// of NULL will remove the specified attribute. Returns the tag given.
struct v3_xml * v3_xml_set_attr(struct v3_xml * xml, const char * name, const char * value) {
    int l = 0;
    int c;

    if (! xml) {
	return NULL;
    }

    while (xml->attr[l] && strcmp(xml->attr[l], name)) {
	l += 2;
    }

    if (! xml->attr[l]) { 
	// not found, add as new attribute
        
	if (! value) {
	    // nothing to do
	    return xml;
	}
       
	if (xml->attr == V3_XML_NIL) { 
	    // first attribute
            xml->attr = V3_Malloc(4 * sizeof(char *));

	    // empty list of malloced names/vals
            xml->attr[1] = strdup(""); 
        } else {
	    xml->attr = tmp_realloc(xml->attr, l * sizeof(char *), (l + 4) * sizeof(char *));
	}

	// set attribute name
        xml->attr[l] = (char *)name; 

	// null terminate attribute list
        xml->attr[l + 2] = NULL; 

        xml->attr[l + 3] = tmp_realloc(xml->attr[l + 1],
				       strlen(xml->attr[l + 1]),
				       (c = strlen(xml->attr[l + 1])) + 2);

	// set name/value as not malloced
        strcpy(xml->attr[l + 3] + c, " "); 

        if (xml->flags & V3_XML_DUP) {
	    xml->attr[l + 3][c] = V3_XML_NAMEM;
	}
    } else if (xml->flags & V3_XML_DUP) {
	// name was strduped
	V3_Free((char *)name); 
    }


    // find end of attribute list
    for (c = l; xml->attr[c]; c += 2); 

    if (xml->attr[c + 1][l / 2] & V3_XML_TXTM) {
	//old val
	V3_Free(xml->attr[l + 1]); 
    }

    if (xml->flags & V3_XML_DUP) {
	xml->attr[c + 1][l / 2] |= V3_XML_TXTM;
    } else {
	xml->attr[c + 1][l / 2] &= ~V3_XML_TXTM;
    }


    if (value) {
	// set attribute value
	xml->attr[l + 1] = (char *)value; 
    } else { 
	// remove attribute
        
	if (xml->attr[c + 1][l / 2] & V3_XML_NAMEM) {
	    V3_Free(xml->attr[l]);
	}

        memmove(xml->attr + l, xml->attr + l + 2, (c - l + 2) * sizeof(char*));

        xml->attr = tmp_realloc(xml->attr, c * sizeof(char *), (c + 2) * sizeof(char *));

	// fix list of which name/vals are malloced
        memmove(xml->attr[c + 1] + (l / 2), xml->attr[c + 1] + (l / 2) + 1,
                (c / 2) - (l / 2)); 
    }

    // clear strdup() flag
    xml->flags &= ~V3_XML_DUP; 

    return xml;
}

// removes a tag along with its subtags without freeing its memory
struct v3_xml * v3_xml_cut(struct v3_xml * xml) {
    struct v3_xml * cur;

    if (! xml) {
	// nothing to do
	return NULL; 
    }

    if (xml->next) {
	// patch sibling list
	xml->next->sibling = xml->sibling; 
    }


    if (xml->parent) { 
	// not root tag

	// find head of subtag list
        cur = xml->parent->child; 

        if (cur == xml) {
	    // first subtag
	    xml->parent->child = xml->ordered; 
	} else { 
	// not first subtag

            while (cur->ordered != xml) {
		cur = cur->ordered;
	    }

	    // patch ordered list
            cur->ordered = cur->ordered->ordered; 

	    // go back to head of subtag list
            cur = xml->parent->child; 

            if (strcmp(cur->name, xml->name)) {
		// not in first sibling list

                while (strcmp(cur->sibling->name, xml->name)) {
                    cur = cur->sibling;
		}

                if (cur->sibling == xml) { 
		    // first of a sibling list
                    cur->sibling = (xml->next) ? xml->next
                                               : cur->sibling->sibling;
                } else {
		    // not first of a sibling list
		    cur = cur->sibling;
		}
            }

            while (cur->next && cur->next != xml) {
		cur = cur->next;
	    }

            if (cur->next) {
		// patch next list
		cur->next = cur->next->next; 
	    }
        } 
   }
    xml->ordered = xml->sibling = xml->next = NULL;
    return xml;
}




/* ************************** */
/* *** XML ENCODING       *** */
/* ************************** */

// Encodes ampersand sequences appending the results to *dst, reallocating *dst
// if length excedes max. a is non-zero for attribute encoding. Returns *dst
static char *ampencode(const char *s, size_t len, char **dst, size_t *dlen,
                      size_t * max, short a)
{
    const char * e;
    
    for (e = s + len; s != e; s++) {
        while (*dlen + 10 > *max) {
	    *dst = tmp_realloc(*dst, *max, *max + V3_XML_BUFSIZE);
	    *max += V3_XML_BUFSIZE;
	}

        switch (*s) {
        case '\0': return *dst;
        case '&': *dlen += sprintf(*dst + *dlen, "&amp;"); break;
        case '<': *dlen += sprintf(*dst + *dlen, "&lt;"); break;
        case '>': *dlen += sprintf(*dst + *dlen, "&gt;"); break;
        case '"': *dlen += sprintf(*dst + *dlen, (a) ? "&quot;" : "\""); break;
        case '\n': *dlen += sprintf(*dst + *dlen, (a) ? "&#xA;" : "\n"); break;
        case '\t': *dlen += sprintf(*dst + *dlen, (a) ? "&#x9;" : "\t"); break;
        case '\r': *dlen += sprintf(*dst + *dlen, "&#xD;"); break;
        default: (*dst)[(*dlen)++] = *s;
        }
    }
    return *dst;
}



// Recursively converts each tag to xml appending it to *s. Reallocates *s if
// its length excedes max. start is the location of the previous tag in the
// parent tag's character content. Returns *s.
static char *toxml_r(struct v3_xml * xml, char **s, size_t *len, size_t *max,
                    size_t start) {
    int i;
    char *txt = (xml->parent) ? xml->parent->txt : "";
    size_t off = 0;

    // parent character content up to this tag
    *s = ampencode(txt + start, xml->off - start, s, len, max, 0);

    while (*len + strlen(xml->name) + 4 > *max) {
	// reallocate s
        *s = tmp_realloc(*s, *max, *max + V3_XML_BUFSIZE);
	*max += V3_XML_BUFSIZE;
    }


    *len += sprintf(*s + *len, "<%s", xml->name); // open tag
    for (i = 0; xml->attr[i]; i += 2) { // tag attributes
        if (v3_xml_attr(xml, xml->attr[i]) != xml->attr[i + 1]) continue;
        while (*len + strlen(xml->attr[i]) + 7 > *max) {
	    // reallocate s
            *s = tmp_realloc(*s, *max, *max + V3_XML_BUFSIZE);
	    *max += V3_XML_BUFSIZE;
	}

        *len += sprintf(*s + *len, " %s=\"", xml->attr[i]);
        ampencode(xml->attr[i + 1], -1, s, len, max, 1);
        *len += sprintf(*s + *len, "\"");
    }

  
    *len += sprintf(*s + *len, ">");

    *s = (xml->child) ? toxml_r(xml->child, s, len, max, 0) //child
                      : ampencode(xml->txt, -1, s, len, max, 0);  //data
    
    while (*len + strlen(xml->name) + 4 > *max) {
	// reallocate s
        *s = tmp_realloc(*s, *max, *max + V3_XML_BUFSIZE);
	*max += V3_XML_BUFSIZE;
    }

    *len += sprintf(*s + *len, "</%s>", xml->name); // close tag

    while (txt[off] && off < xml->off) off++; // make sure off is within bounds
    return (xml->ordered) ? toxml_r(xml->ordered, s, len, max, off)
                          : ampencode(txt + off, -1, s, len, max, 0);
}

// Converts an xml structure back to xml. Returns a string of xml data that
// must be freed.
char * v3_xml_tostr(struct v3_xml * xml) {
    struct v3_xml * p = (xml) ? xml->parent : NULL;
    struct v3_xml * o = (xml) ? xml->ordered : NULL;
    struct v3_xml_root * root = (struct v3_xml_root *)xml;
    size_t len = 0, max = V3_XML_BUFSIZE;
    char *s = strcpy(V3_Malloc(max), "");

    if (! xml || ! xml->name) return tmp_realloc(s, max, len + 1);
    while (root->xml.parent) root = (struct v3_xml_root *)root->xml.parent; // root tag


    xml->parent = xml->ordered = NULL;
    s = toxml_r(xml, &s, &len, &max, 0);
    xml->parent = p;
    xml->ordered = o;


    return tmp_realloc(s, max, len + 1);
}
