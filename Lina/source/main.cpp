//	Lina - HTML compiler for VirtualDub help system
//	Copyright (C) 1998-2003 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#pragma warning(disable: 4786)

#include <sys/stat.h>
#include <direct.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <utility>

using namespace std;

///////////////////////////////////////////////////////////////////////////

set<string>	g_tagSetSupportingCDATA;
list<pair<string, bool> > g_truncateURLs;

///////////////////////////////////////////////////////////////////////////

struct Tag;
const Tag *root();

struct Attribute {
	string name, value;
	bool no_value;
};

struct TagLocation {
	string	name;
};

struct Tag {
	TagLocation *location;
	int lineno;
	string name;
	list<Attribute> attribs;
	list<Tag *> children;
	bool is_text;
	bool is_control;

	const Attribute *attrib(string s) const {
		list<Attribute>::const_iterator it(attribs.begin()), itEnd(attribs.end());

		for(; it!=itEnd; ++it)
			if ((*it).name == s)
				return &*it;
		return NULL;
	}

	const Tag *resolve_path(const string path, string& name) const {
		string::size_type p = path.find('/');

		if (!p) {
			return root()->resolve_path(path.substr(1), name);
		} else if (p != string::npos) {
			const Tag *t = child(path.substr(0, p));

			if (!t)
				return NULL;

			return t->resolve_path(path.substr(p+1), name);
		} else {
			name = path;
			return this;
		}
	}

	const Tag *child(string s) const {
		string name;
		const Tag *parent = resolve_path(s, name);

		list<Tag *>::const_iterator it(children.begin()), itEnd(children.end());

		for(; it!=itEnd; ++it)
			if (!(*it)->is_text) {
				if ((*it)->name == "lina:data") {
					const Tag *t = (*it)->child(name);
					if (t)
						return t;
				}
				if ((*it)->name == name)
					return *it;
			}
		return NULL;
	}

	bool supports_cdata() const {
		return g_tagSetSupportingCDATA.find(name) != g_tagSetSupportingCDATA.end();
	}

	static void set_supports_cdata(string tagname, bool supports_cdata) {
		if (supports_cdata)
			g_tagSetSupportingCDATA.insert(tagname);
		else
			g_tagSetSupportingCDATA.erase(tagname);
	}
};

struct Context {
	list<const Tag *> stack;
	list<const Tag *> invocation_stack;
	list<Tag *> construction_stack;
	int pre_count;
	int cdata_count;
	bool eat_next_space;
	bool holding_space;

	Context() : pre_count(0), cdata_count(0), eat_next_space(true), holding_space(false) {}

	const Tag *find_tag(string name) {
		list<const Tag *>::reverse_iterator it(invocation_stack.rbegin()), itEnd(invocation_stack.rend());
		const Tag *t = NULL;
		
		for(; it!=itEnd; ++it) {
			t = (*it)->child(name);
			if (t)
				break;
			if (!name.empty() && name[0]=='/')
				break;
		}

		return t;
	}
};

void output_tag(Context& ctx, string *out, const Tag& tag);
void output_tag_contents(Context& ctx, std::string *out, const Tag& tag);

//////////////////////////////////////////////////////////////

struct FileContext {
	TagLocation *tagloc;
	FILE *f;
	string name;
	int lineno;
};

list<TagLocation> g_tagLocations;
list<FileContext> g_fileStack;
TagLocation *g_pTagLocation;
FILE *g_file;
string g_name;
string g_outputDir;
int g_line = 1;

Tag *g_pBaseTag;
list<Tag> g_tagHeap;
map<string, Tag *> g_macros;

typedef map<string, string> tFileCopies;
tFileCopies g_fileCopies;

//////////////////////////////////////////////////////////////

const Tag *root() {
	return g_pBaseTag;
}

//////////////////////////////////////////////////////////////

void error(const char *format, ...) {
	va_list val;

	printf("%s(%d): Error! ", g_name.c_str(), g_line);

	va_start(val, format);
	vprintf(format, val);
	va_end(val);
	putchar('\n');
	exit(10);
}

void error(const Context& ctx, const char *format, ...) {
	va_list val;

	list<const Tag *>::const_reverse_iterator it(ctx.stack.rbegin()), itEnd(ctx.stack.rend());

	printf("%s(%d): Error! ", (*it)->location->name.c_str(), (*it)->lineno);

	va_start(val, format);
	vprintf(format, val);
	va_end(val);
	putchar('\n');

	int indent = 3;
	for(++it; it!=itEnd; ++it) {
		const Tag& tag = **it;
		printf("%*c%s(%d): while processing tag <%s>\n", indent, ' ', tag.location->name.c_str(), tag.lineno, tag.name.c_str());
		indent += 3;
	}

	indent = 3;
	for(it=ctx.invocation_stack.rbegin(), itEnd=ctx.invocation_stack.rend(); it!=itEnd; ++it) {
		const Tag& tag = **it;
		printf("%*c%s(%d): while invoked from tag <%s> (%d children)\n", indent, ' ', tag.location->name.c_str(), tag.lineno, tag.name.c_str(), tag.children.size());
		indent += 3;
	}

	exit(10);
}

void unexpected(int c) {
	error("unexpected character '%c'", (char)c);
}

void construct_path(const std::string& dstfile) {
	int idx = -1;

	for(;;) {
		int pos = dstfile.find_first_of("\\/", idx+1);

		if (pos == string::npos)
			break;

		string partialpath(dstfile.substr(0, pos));
		struct _stat buffer;

		if (-1 == _stat(partialpath.c_str(), &buffer)) {
			printf("creating: %s\n", partialpath.c_str());
			_mkdir(partialpath.c_str());
		}

		idx = pos;
	}
}

void copy_file(const std::string& dstfile, const std::string& srcfile) {
	printf("copying: %s -> %s\n", srcfile.c_str(), dstfile.c_str());

	FILE *fs = fopen(srcfile.c_str(), "rb");

	if (!fs)
		error("couldn't open source file \"%s\"", srcfile.c_str());

	string filename(g_outputDir);

	if (!filename.empty()) {
		char c = filename[filename.size()-1];
		if (c != '/' && c != '\\')
			filename += '/';
	}

	filename += dstfile;

	construct_path(filename);

	FILE *fd = fopen(filename.c_str(), "wb");
	if (!fd)
		error("couldn't create \"%s\"", filename.c_str());

	fseek(fs, 0, SEEK_END);
	std::vector<char> data(ftell(fs));
	fseek(fs, 0, SEEK_SET);
	if (1 != fread(&data.front(), data.size(), 1, fs))
		error("couldn't read from \"%s\"", srcfile.c_str());
	fclose(fs);

	if (1 != fwrite(&data.front(), data.size(), 1, fd) || fclose(fd))
		error("couldn't write to \"%s\"", dstfile.c_str());
}

bool is_true(const std::string& name) {
	return name.empty() || name[0]=='y' || name[0]=='Y';
}

void push_file(const char *fname) {
	FILE *f = fopen(fname, "r");
	if (!f)
		error("cannot open \"%s\"", fname);

	if (g_file) {
		FileContext fc;

		fc.f = g_file;
		fc.name = g_name;
		fc.lineno = g_line;
		fc.tagloc = g_pTagLocation;
		g_fileStack.push_back(fc);
	}

	TagLocation tl;
	tl.name = fname;
	g_tagLocations.push_back(tl);

	g_pTagLocation = &g_tagLocations.back();
	g_file = f;
	g_name = fname;
	g_line = 1;

	printf("Processing: %s\n", fname);
}

bool pop_file() {
	if (g_file)
		fclose(g_file);

	if (g_fileStack.empty()) {
		g_file = NULL;
	} else {
		const FileContext& fc = g_fileStack.back();
		g_file = fc.f;
		g_name = fc.name;
		g_line = fc.lineno;
		g_pTagLocation = fc.tagloc;
		g_fileStack.pop_back();
	}
	return g_file!=0;
}

int next() {
	int c;

	do {
		c = getc(g_file);
	} while(c == EOF && pop_file());

	if (c == '\n')
		++g_line;

	return c;
}

int next_required() {
	int c = next();
	if (c == EOF)
		error("unexpected end of file");
	return c;
}

bool istagchar(int c) {
	return isalnum(c) || c=='-' || c==':';
}

bool issafevaluechar(int c) {
	return isalnum(c) || c=='-';
}

Tag *parse_alloc_tag() {
	g_tagHeap.push_back(Tag());
	Tag *t = &g_tagHeap.back();

	t->location = g_pTagLocation;
	t->lineno = g_line;
	return t;
}

Tag *parse_inline(int& c) {
	Tag& tag = *parse_alloc_tag();
	bool last_was_space = false;

	tag.is_text = true;
	tag.is_control = false;

	do {
		tag.name += c;
		c = next_required();
	} while(c != '<');

	return &tag;
}

// parse_tag
//
// Assumes that starting < has already been parsed.
Tag *parse_tag() {
	Tag& tag = *parse_alloc_tag();
	bool closed = false;
	int c;

	tag.is_control = false;
	tag.is_text = false;

	c = next_required();
	if (isspace(c))
		do {
			c = next_required();
		} while(isspace(c));

	if (c=='?' || c=='!') {
		tag.is_text = true;
		tag.is_control = true;
		tag.name = "<";
		tag.name += c;

		int bracket_count = 1;
		do {
			c = next_required();
			tag.name += c;

			if (c == '<')
				++bracket_count;
			else if (c == '>')
				--bracket_count;
		} while(bracket_count);
		return &tag;
	} else if (c == '/') {
		tag.name += c;
		c = next_required();
	}

	do {
		tag.name += tolower(c);
		c = next_required();
	} while(istagchar(c));

	if (tag.name[0] == '/')
		closed = true;

	while(c != '>') {
		if (c == '/' || c=='?') {
			closed = true;
			c = next_required();
		} else if (istagchar(c)) {
			tag.attribs.push_back(Attribute());
			Attribute& att = tag.attribs.back();

			do {
				att.name += tolower(c);
				c = next_required();
			} while(istagchar(c));

			while(isspace(c))
				c = next_required();

			att.no_value = true;

			if (c == '=') {
				att.no_value = false;
				do {
					c = next_required();
				} while(isspace(c));

				if (c == '"') {
					c = next_required();
					while(c != '"') {
						att.value += c;
						c = next_required();
					}
					c = next_required();
				} else {
					do {
						att.value += c;
						c = next_required();
					} while(istagchar(c));
				}
			}

		} else if (isspace(c)) {
			c = next_required();
		} else
			unexpected(c);
	}

	if (!closed) {
		c = next_required();
		for(;;) {
			Tag *p;
			if (c == '<') {
				p = parse_tag();

				if (p && !p->name.empty() && p->name[0] == '/') {
					if ((string("/") + tag.name) != p->name)
						error("closing tag <%s> doesn't match opening tag <%s> on line %d", p->name.c_str(), tag.name.c_str(), tag.lineno);
					break;
				}
				c = next_required();
			} else {
				p = parse_inline(c);
			}

			if (p)
				tag.children.push_back(p);
		}
	}

	// Check for a macro or include and whisk it away if so.

	if (tag.name == "lina:macro") {
		const Attribute *a = tag.attrib("name");

		if (!a)
			error("macro definition must have NAME attribute");

		g_macros[a->value] = &tag;

		return NULL;
	} else if (tag.name == "lina:include") {
		const Attribute *a = tag.attrib("file");

		if (!a)
			error("<lina:include> must specify FILE");

		push_file(a->value.c_str());

		return NULL;
	}

	return &tag;
}

void dump_parse_tree(const Tag& tag, int indent = 0) {
	if (tag.is_text) {
	} else if (tag.children.empty()) {
		printf("%*c<%s/>\n", indent, ' ', tag.name.c_str());
	} else {
		printf("%*c<%s>\n", indent, ' ', tag.name.c_str());

		list<Tag *>::const_iterator it(tag.children.begin()), itEnd(tag.children.end());
		for(; it!=itEnd; ++it) {
			dump_parse_tree(**it, indent+3);
		}

		printf("%*c</%s>\n", indent, ' ', tag.name.c_str());
	}
}

////////////////////////////////////////////////////////////////////////////////

void output_tag_attributes(std::string& out, const Tag& tag) {
	list<Attribute>::const_iterator itAtt(tag.attribs.begin()), itAttEnd(tag.attribs.end());
	bool is_anchor = (tag.name == "a");
	
	for(; itAtt!=itAttEnd; ++itAtt) {
		const Attribute& att = *itAtt;

		out += ' ';
		out += att.name;

		if (!att.no_value) {
			string::const_iterator its(att.value.begin()), itsEnd(att.value.end());
			for(;its!=itsEnd; ++its)
				if (!issafevaluechar(*its))
					break;

			string value(att.value);

			if (is_anchor && att.name == "href") {
				list<pair<string, bool> >::const_iterator it(g_truncateURLs.begin()), itEnd(g_truncateURLs.end());

				for(; it!=itEnd; ++it) {
					const pair<string, bool>& entry = *it;

					if (value.length() >= entry.first.length() && !value.compare(0, entry.first.length(), entry.first)) {
						if (entry.second) {
							int l = value.length();

							while(l>0) {
								char c = value[--l];

								if (c == '/' || c == ':')
									break;
								if (c == '.') {
									if (value.substr(l+1, string::npos) == "html")
										value.erase(l, string::npos);
									break;
								}
							}
							printf("truncated link: %s\n", value.c_str());
						}
						break;
					}
				}
			}

			if (att.value.empty() || its!=itsEnd) {
				out += "=\"";
				out += value;
				out += '"';
			} else {
				out += '=';
				out += value;
			}
		}
	}
}

void output_tag_contents(Context& ctx, std::string *out, const Tag& tag) {
	static int recursion_depth = 0;

	++recursion_depth;

	if (recursion_depth > 64)
		error(ctx, "recursion exceeded limits");

	list<Tag *>::const_iterator it(tag.children.begin()), itEnd(tag.children.end());
	for(; it!=itEnd; ++it) {
		output_tag(ctx, out, **it);
	}

	--recursion_depth;
}

void output_standard_tag(Context& ctx, std::string *out, const Tag& tag) {
	if (!tag.is_text && tag.name == "pre")
		++ctx.pre_count;

	if (out && (tag.is_control || !tag.is_text)) {
		if (ctx.holding_space && ctx.cdata_count) {
			if (!ctx.eat_next_space) {
				*out += ' ';
			}
			ctx.eat_next_space = false;
			ctx.holding_space = false;
		}
	}

	if (!ctx.construction_stack.empty()) {
		Tag *new_tag = parse_alloc_tag();

		new_tag->location	= tag.location;
		new_tag->lineno		= tag.lineno;
		new_tag->name		= tag.name;
		new_tag->attribs	= tag.attribs;
		new_tag->is_text	= tag.is_text;
		new_tag->is_control	= tag.is_control;

		ctx.construction_stack.back()->children.push_back(new_tag);
		ctx.construction_stack.push_back(new_tag);

		output_tag_contents(ctx, out, tag);

		ctx.construction_stack.pop_back();
	} else if (tag.is_text) {
		if (out) {
			if (tag.is_control) {
				*out += tag.name;
			} else if (ctx.cdata_count) {
				if (ctx.pre_count) {
					*out += tag.name;
				} else {
					string::const_iterator it(tag.name.begin()), itEnd(tag.name.end());

					for(; it!=itEnd; ++it) {
						const char c = *it;

						if (isspace(c)) {
							ctx.holding_space = true;
						} else {
							if (ctx.eat_next_space)
								ctx.eat_next_space = false;
							else if (ctx.holding_space)
								*out += ' ';

							ctx.holding_space = false;
							*out += c;
						}
					}
				}
			} else {
				string::const_iterator it(tag.name.begin()), itEnd(tag.name.end());

				for(; it!=itEnd; ++it) {
					const char c = *it;

					if (!isspace(c))
						error(ctx, "inline text not allowed");
				}
			}
		}
	} else {
		bool cdata = tag.supports_cdata();

		if (cdata) {
			if (!ctx.cdata_count) {
				ctx.holding_space = false;
				ctx.eat_next_space = true;
			}
			++ctx.cdata_count;
		}

		if (!out) {
			output_tag_contents(ctx, out, tag);
		} else if (tag.children.empty()) {
			*out += '<';
			*out += tag.name;
			output_tag_attributes(*out, tag);
			*out += '>';
		} else {
			*out += '<';
			*out += tag.name;
			output_tag_attributes(*out, tag);
			*out += '>';

			output_tag_contents(ctx, out, tag);

			*out += "</";
			*out += tag.name;
			*out += '>';
		}

		if (cdata)
			--ctx.cdata_count;
	}
	if (!tag.is_text && tag.name == "pre")
		--ctx.pre_count;
}

string HTMLize(const string& s) {
	string::const_iterator it(s.begin()), itEnd(s.end());
	string t;

	for(; it!=itEnd; ++it) {
		char c = *it;

		switch(c) {
		case '"':	t.append("&quot;"); break;
		case '<':	t.append("&lt;"); break;
		case '>':	t.append("&gt;"); break;
		case '&':	t.append("&amp;"); break;
		default:	t += c; break;
		}
	}

	return t;
}

void output_source_tags(Context& ctx, std::string *out, const Tag& tag) {
	string s;

	if (tag.is_text)
		s = tag.name;
	else if (tag.children.empty()) {
		s = '<';
		s += tag.name;
		output_tag_attributes(s, tag);
		s += '>';
	} else {
		s = '<';
		s += tag.name;
		output_tag_attributes(s, tag);
		s += '>';

		out->append(HTMLize(s));

		out->append("<ul marker=none>");

		list<Tag *>::const_iterator itBegin(tag.children.begin()), it(itBegin), itEnd(tag.children.end());
		for(; it!=itEnd; ++it) {
		out->append("<li>");
			output_source_tags(ctx, out, **it);
		out->append("</li>");
		}

		out->append("</ul>");

		s = "</";
		s += tag.name;
		s += '>';
	}

	out->append(HTMLize(s));

	if (!tag.is_text)
		out->append("<br>");
}

void dump_stack(Context& ctx) {
	list<const Tag *>::reverse_iterator it(ctx.stack.rbegin()), itEnd(ctx.stack.rend());

	printf("Current execution stack:\n");
	int indent = 3;
	for(++it; it!=itEnd; ++it) {
		const Tag& tag = **it;
		printf("%*c%s(%d): processing <%s>\n", indent, ' ', tag.location->name.c_str(), tag.lineno, tag.name.c_str());
		indent += 3;
	}

	indent = 3;
	list<Tag *>::reverse_iterator it2(ctx.construction_stack.rbegin()), it2End(ctx.construction_stack.rend());
	for(; it2!=it2End; ++it2) {
		const Tag& tag = **it2;
		printf("%*c%s(%d): while creating tag <%s>\n", indent, ' ', tag.location->name.c_str(), tag.lineno, tag.name.c_str());
		indent += 3;
	}

	indent = 3;
	for(it=ctx.invocation_stack.rbegin(), itEnd=ctx.invocation_stack.rend(); it!=itEnd; ++it) {
		const Tag& tag = **it;
		printf("%*c%s(%d): while invoked from tag <%s>\n", indent, ' ', tag.location->name.c_str(), tag.lineno, tag.name.c_str());
		indent += 3;
	}
}

void output_special_tag(Context& ctx, std::string *out, const Tag& tag) {
	if (tag.name == "lina:fireball") {
		const Attribute *a1 = tag.attrib("src");
		const Attribute *a2 = tag.attrib("dst");

		if (!a1 || !a2)
			error(ctx, "<lina:fireball> requires SRC and DST attributes");

		g_fileCopies[a2->value] = a1->value;
	} else if (tag.name == "lina:write") {
		const Attribute *a = tag.attrib("file");

		if (!a)
			error(ctx, "<lina:write> must specify FILE");

		string s;

		output_tag_contents(ctx, &s, tag);

		string filename(g_outputDir);

		if (!filename.empty()) {
			char c = filename[filename.size()-1];
			if (c != '/' && c != '\\')
				filename += '/';
		}

		filename += a->value;

		FILE *f = fopen(filename.c_str(), "wb");
		if (!f)
			error(ctx, "couldn't create \"%s\"", a->value.c_str());
		fwrite(s.data(), s.length(), 1, f);
		fclose(f);

		printf("created file: %s\n", a->value.c_str());
	} else if (tag.name == "lina:body") {

//		printf("outputting:\n");
//		dump_parse_tree(*ctx.invocation_stack.back(), 4);

		output_tag_contents(ctx, out, *ctx.invocation_stack.back());
	} else if (tag.name == "lina:tag") {
		const Attribute *a = tag.attrib("name");
		if (!a)
			error(ctx, "<lina:tag> must have NAME attribute");

		ctx.construction_stack.push_back(parse_alloc_tag());
		Tag *new_tag = ctx.construction_stack.back();

		new_tag->name = a->value;
		new_tag->is_text = false;
		new_tag->is_control = false;

		output_tag_contents(ctx, NULL, tag);

		ctx.construction_stack.pop_back();
		output_tag(ctx, out, *new_tag);
	} else if (tag.name == "lina:arg") {
		if (!out && ctx.construction_stack.empty())
			error(ctx, "<lina:arg> can only be used in an output context");
		const Attribute *a = tag.attrib("name");
		if (!a)
			error(ctx, "<lina:arg> must have NAME attribute");

		if (ctx.invocation_stack.empty())
			error(ctx, "<lina:arg> can only be used during macro expansion");

		const Tag& macrotag = *ctx.invocation_stack.back();
		const Attribute *a2 = macrotag.attrib(a->value);
		if (!a2)
			error(ctx, "macro invocation <%s> does not have an attribute \"%s\"", macrotag.name.c_str(), a->value.c_str());

		if (out) {
			*out += a2->value;

			ctx.eat_next_space = false;
			ctx.holding_space = false;
		} else {
			Tag *t = parse_alloc_tag();

			t->is_control = false;
			t->is_text = true;
			t->name = a2->value;

			ctx.construction_stack.back()->children.push_back(t);
		}
	} else if (tag.name == "lina:if-arg") {
		const Attribute *a = tag.attrib("name");
		if (!a)
			error(ctx, "<lina:if-arg> must have NAME attribute");

		if (ctx.invocation_stack.empty())
			error(ctx, "<lina:if-arg> can only be used during macro expansion");

		const Tag& macrotag = *ctx.invocation_stack.back();
		const Attribute *a2 = macrotag.attrib(a->value);
		if (a2)
			output_tag_contents(ctx, out, tag);
	} else if (tag.name == "lina:if-not-arg") {
		const Attribute *a = tag.attrib("name");
		if (!a)
			error(ctx, "<lina:if-not-arg> must have NAME attribute");

		if (ctx.invocation_stack.empty())
			error(ctx, "<lina:if-not-arg> can only be used during macro expansion");

		const Tag& macrotag = *ctx.invocation_stack.back();
		const Attribute *a2 = macrotag.attrib(a->value);
		if (!a2)
			output_tag_contents(ctx, out, tag);
	} else if (tag.name == "lina:attrib") {
		if (ctx.construction_stack.empty())
			error(ctx, "<lina:attrib> can only be used in a <lina:tag> element");

		const Attribute *a = tag.attrib("name");
		if (!a)
			error(ctx, "<lina:attrib> must have NAME attribute");

		std::string s;
		std::list<Tag *> tempStack;
		ctx.construction_stack.swap(tempStack);
		++ctx.cdata_count;
		++ctx.pre_count;
		bool bHoldingSpace = ctx.holding_space;
		bool bEatNextSpace = ctx.eat_next_space;
		ctx.holding_space = false;
		ctx.eat_next_space = true;
		output_tag_contents(ctx, &s, tag);
		ctx.holding_space = bHoldingSpace;
		ctx.eat_next_space = bEatNextSpace;
		--ctx.pre_count;
		--ctx.cdata_count;
		ctx.construction_stack.swap(tempStack);

		Tag *t = ctx.construction_stack.back();
		Attribute new_att;
		if (tag.attrib("novalue")) {
			new_att.no_value = true;
		} else {
			new_att.no_value = false;
			new_att.value = s;
		}
		new_att.name = a->value;
		t->attribs.push_back(new_att);
	} else if (tag.name == "lina:pull") {
		if (ctx.invocation_stack.empty())
			error(ctx, "<lina:pull> can only be used during macro expansion");

		const Attribute *a = tag.attrib("name");
		if (!a)
			error(ctx, "<lina:pull> must have NAME attribute");

		const Tag *t = ctx.find_tag(a->value);
		
		if (!t)
			error(ctx, "cannot find tag <%s> referenced in <lina:pull>", a->value.c_str());

		output_tag_contents(ctx, out, *t);		
	} else if (tag.name == "lina:for-each") {
		const Attribute *a = tag.attrib("name");
		if (!a)
			error(ctx, "<lina:for-each> must have NAME attribute");
		
		string node_name;
		const Tag *parent;
		if (ctx.invocation_stack.empty()) {
			if (!a->value.empty() && a->value[0] == '/')
				parent = root()->resolve_path(a->value.substr(1), node_name);
			else
				error(ctx, "path must be absolute if not in macro context");
		} else {
			list<const Tag *>::reverse_iterator it(ctx.invocation_stack.rbegin()), itEnd(ctx.invocation_stack.rend());
			
			for(; it!=itEnd; ++it) {
				parent = (*it)->resolve_path(a->value, node_name);
				if(parent)
					break;
				if (!a->value.empty() && a->value[0] == '/')
					break;
			}
		}

		if (!parent)
			error(ctx, "cannot resolve path \"%s\"", a->value.c_str());

		list<Tag *>::const_iterator it2(parent->children.begin()), it2End(parent->children.end());

		ctx.invocation_stack.push_back(NULL);
		for(; it2!=it2End; ++it2) {
			if ((*it2)->name == node_name) {
				ctx.invocation_stack.back() = *it2;
				output_tag_contents(ctx, out, tag);
			}
		}
		ctx.invocation_stack.pop_back();
	} else if (tag.name == "lina:apply") {
		const Attribute *a = tag.attrib("name");
		if (!a)
			error(ctx, "<lina:apply> must have NAME attribute");

		map<string, Tag *>::const_iterator it(g_macros.find(a->value));

		if (it == g_macros.end())
			error(ctx, "macro \"%s\" undeclared", a->value.c_str());
		
		list<Tag *>::const_iterator it2(tag.children.begin()), it2End(tag.children.end());

		ctx.invocation_stack.push_back(NULL);
		for(; it2!=it2End; ++it2) {
			if (!(*it2)->is_text) {
				ctx.invocation_stack.back() = *it2;
				output_tag_contents(ctx, out, *(*it).second);
			}
		}
		ctx.invocation_stack.pop_back();
	} else if (tag.name == "lina:if-present") {
		if (ctx.invocation_stack.empty())
			error(ctx, "<lina:if-present> can only be used during macro expansion");
		const Attribute *a = tag.attrib("name");
		if (!a)
			error(ctx, "<lina:if-present> must have NAME attribute");

		const Tag *t = ctx.find_tag(a->value);
		if (t)
			output_tag_contents(ctx, out, tag);
	} else if (tag.name == "lina:if-not-present") {
		if (ctx.invocation_stack.empty())
			error(ctx, "<lina:if-not-present> can only be used during macro expansion");
		const Attribute *a = tag.attrib("name");
		if (!a)
			error(ctx, "<lina:if-not-present> must have NAME attribute");

		const Tag *t = ctx.find_tag(a->value);
		if (!t)
			output_tag_contents(ctx, out, tag);
	} else if (tag.name == "lina:pre") {
		++ctx.pre_count;
		++ctx.cdata_count;
		if (!out)
			output_standard_tag(ctx, out, tag);
		else {
			output_tag_contents(ctx, out, tag);
		}
		--ctx.cdata_count;
		--ctx.pre_count;
	} else if (tag.name == "lina:cdata") {
		++ctx.cdata_count;
		if (!out)
			output_standard_tag(ctx, out, tag);
		else
			output_tag_contents(ctx, out, tag);
		--ctx.cdata_count;
	} else if (tag.name == "lina:delay") {
		list<Tag *>::const_iterator it(tag.children.begin()), itEnd(tag.children.end());
		for(; it!=itEnd; ++it) {
			output_standard_tag(ctx, out, **it);
		}
	} else if (tag.name == "lina:dump-stack") {
		dump_stack(ctx);
	} else if (tag.name == "lina:replace") {
		const Attribute *a = tag.attrib("from");
		if (!a || a->no_value)
			error(ctx, "<lina:replace> must have FROM attribute");
		const Attribute *a2 = tag.attrib("to");
		if (!a2 || a2->no_value)
			error(ctx, "<lina:replace> must have TO attribute");

		const string& x = a->value;
		const string& y = a2->value;

		string s, t;
		string::size_type i = 0;

		output_tag_contents(ctx, &s, tag);

		for(;;) {
			string::size_type j = s.find(x, i);
			if (j != i)
				t.append(s, i, j-i);
			if (j == string::npos)
				break;
			t.append(y);
			i = j + x.size();
		}

		Tag *new_tag = parse_alloc_tag();

		new_tag->is_text = true;
		new_tag->is_control = false;
		new_tag->name = t;

		output_tag(ctx, out, *new_tag);
	} else if (tag.name == "lina:set-option") {
		const Attribute *a_name = tag.attrib("name");
		if (!a_name)
			error(ctx, "<lina:set-option> must have NAME attribute");

		if (a_name->value == "link-truncate") {
			const Attribute *a_val = tag.attrib("baseurl");
			if (!a_val || a_val->no_value)
				error(ctx, "option \"link-truncate\" requires BASEURL attribute");

			bool bTruncate = !tag.attrib("notruncate");

			g_truncateURLs.push_back(make_pair(a_val->value, bTruncate));
		} else if (a_name->value == "output-dir") {
			const Attribute *a_val = tag.attrib("target");
			if (!a_val || a_val->no_value)
				error(ctx, "option \"output-dir\" requires TARGET attribute");

			g_outputDir = a_val->value;
		} else if (a_name->value == "tag-info") {
			const Attribute *a_tagname = tag.attrib("tag");
			if (!a_tagname || a_tagname->no_value)
				error(ctx, "option \"tag-info\" requires TAG attribute");

			const Attribute *a_cdata = tag.attrib("cdata");

			if (!a_cdata || a_cdata->no_value)
				error(ctx, "option \"tag-info\" requires CDATA attribute");

			Tag::set_supports_cdata(a_tagname->value, is_true(a_cdata->value));
		} else
			error(ctx, "option \"%s\" unknown\n", a_name->value.c_str());

	} else if (tag.name == "lina:data") {
		// do nothing
	} else if (tag.name == "lina:source") {
		if (out) {
			list<Tag *>::const_iterator itBegin(tag.children.begin()), it(itBegin), itEnd(tag.children.end());
			for(; it!=itEnd; ++it) {
				output_source_tags(ctx, out, **it);
			}
		}
	} else {
		string macroName(tag.name, 5, string::npos);
		map<string, Tag *>::const_iterator it = g_macros.find(macroName);

		if (it == g_macros.end())
			error(ctx, "macro <lina:%s> not found", macroName.c_str());

//		dump_stack(ctx);
//		printf("executing macro: %s (%s:%d)\n", tag.name.c_str(), tag.location->name.c_str(), tag.lineno);

		ctx.invocation_stack.push_back(&tag);
		output_tag_contents(ctx, out, *(*it).second);
		ctx.invocation_stack.pop_back();

//		printf("exiting macro: %s (%s:%d)\n", tag.name.c_str(), tag.location->name.c_str(), tag.lineno);
	}
}

void output_tag(Context& ctx, std::string *out, const Tag& tag) {
	ctx.stack.push_back(&tag);

	if (!tag.is_text && !tag.name.compare(0,5,"lina:")) {
		output_special_tag(ctx, out, tag);
	} else {
		output_standard_tag(ctx, out, tag);
	}

	ctx.stack.pop_back();
}

int main(int argc, char **argv) {
	int c;

	Tag::set_supports_cdata("p",		true);
	Tag::set_supports_cdata("h1",		true);
	Tag::set_supports_cdata("h2",		true);
	Tag::set_supports_cdata("h3",		true);
	Tag::set_supports_cdata("h4",		true);
	Tag::set_supports_cdata("h5",		true);
	Tag::set_supports_cdata("h6",		true);
	Tag::set_supports_cdata("td",		true);
	Tag::set_supports_cdata("th",		true);
	Tag::set_supports_cdata("li",		true);
	Tag::set_supports_cdata("style",	true);
	Tag::set_supports_cdata("script",	true);
	Tag::set_supports_cdata("title",	true);
	Tag::set_supports_cdata("div",		true);
	Tag::set_supports_cdata("dt",		true);
	Tag::set_supports_cdata("dd",		true);


	push_file(argv[1]);

	while((c = next()) != EOF) {
		if (c == '<') {
			Tag *p = parse_tag();
			if (p && !p->is_text) {
				if (!g_pBaseTag)
					g_pBaseTag = p;
				else
					error("multiple high-level tags detected (first is <%s>, second is <%s>)", root()->name.c_str(), p->name.c_str());
			}
		}
	}

//	dump_parse_tree(*root());

	Context ctx;

	output_tag(ctx, NULL, *root());

	// copy files

	tFileCopies::const_iterator it(g_fileCopies.begin()), itEnd(g_fileCopies.end());

	for(; it!=itEnd; ++it) {
		const tFileCopies::value_type& info = *it;

		copy_file(info.first, info.second);
	}

	printf("No errors.\n");
	return 0;
}
