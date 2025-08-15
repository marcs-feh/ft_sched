#include "base.hpp"

struct INI_Document {
	INI_Section* first_section;
	INI_Section* last_section;
};

struct INI_Section {
	String name;
	INI_Entry* first_entry;
	INI_Entry* last_entry;

	INI_Section* next;
};

struct INI_Entry {
	String key;
	String value;
	INI_Entry* next;
};

struct INI_Parser {
	String source;
	usize current;
	Arena* arena;
};

void ini_skip_whitespace(INI_Parser* parser){
	constexpr auto is_whitespace = [](rune c) -> bool {
		return c == ' ' || c == '\t' || c == '\r' || c == '\n';
	};

	for(; parser->current < source.len; parser->current += 1){
		if(!is_whitespace(parser->source[parser->current])){
			break;
		}
	}
}

void ini_consume_comment(INI_Parser* p){
	for(; parser->current < source.len; parser->current += 1){
		if(parser->source[parser->current] == '\n'){
			break;
		}
	}
}

INI_Section ini_consume_section(INI_Parser* p){
	assert(p->source[p->current] == '[', "");
}

