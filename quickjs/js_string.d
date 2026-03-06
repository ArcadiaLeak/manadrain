module quickjs.js_string;

interface JSString {
	JSString8 str8();
	JSString16 str16();
}

class JSString8 : JSString {
	JSString8 str8() => this;
	JSString16 str16() => null;
}

class JSString16 : JSString {
	JSString8 str8() => null;
	JSString16 str16() => this;
}
