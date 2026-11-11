from __future__ import annotations

from .ir import DefBlock, FieldDef, SchemaDoc, TypeRef


class Parser:
    def __init__(self, text: str) -> None:
        self.text = text
        self.pos = 0

    def eof(self) -> bool:
        return self.pos >= len(self.text)

    def peek(self) -> str:
        if self.eof():
            return "\0"
        return self.text[self.pos]

    def get(self) -> str:
        if self.eof():
            return "\0"
        ch = self.text[self.pos]
        self.pos += 1
        return ch

    def expect(self, ch: str) -> None:
        if self.peek() != ch:
            raise ValueError(f"expect '{ch}' at position {self.pos}")
        self.pos += 1

    def skip_ws_and_comments(self) -> None:
        while not self.eof():
            ch = self.peek()
            if ch.isspace():
                self.pos += 1
                continue
            if ch == "#":
                while not self.eof() and self.peek() != "\n":
                    self.pos += 1
                continue
            if ch == "/" and self.pos + 1 < len(self.text) and self.text[self.pos + 1] == "/":
                self.pos += 2
                while not self.eof() and self.peek() != "\n":
                    self.pos += 1
                continue
            break

    def parse_identifier(self) -> str:
        self.skip_ws_and_comments()
        if self.peek() == '"':
            return self.parse_string()
        out = []
        while not self.eof():
            ch = self.peek()
            if ch.isalnum() or ch in "_-.:/":
                out.append(self.get())
            else:
                break
        if not out:
            raise ValueError(f"empty identifier at position {self.pos}")
        return "".join(out)

    def parse_string(self) -> str:
        self.expect('"')
        out = []
        while not self.eof():
            ch = self.get()
            if ch == '"':
                return "".join(out)
            if ch == "\\":
                esc = self.get()
                mapping = {"n": "\n", "r": "\r", "t": "\t", '"': '"', "\\": "\\"}
                out.append(mapping.get(esc, esc))
                continue
            out.append(ch)
        raise ValueError("unterminated string")

    def parse_integer(self) -> int:
        self.skip_ws_and_comments()
        out = []
        while not self.eof() and self.peek().isdigit():
            out.append(self.get())
        if not out:
            raise ValueError(f"empty integer at position {self.pos}")
        return int("".join(out))

    def parse_type_ref(self) -> TypeRef:
        self.skip_ws_and_comments()
        if self.peek() == "[":
            self.expect("[")
            name = self.parse_identifier()
            self.skip_ws_and_comments()
            self.expect("]")
            out = TypeRef(category="array", name=name)
        elif self.peek() == "{":
            self.expect("{")
            key_name = self.parse_identifier()
            self.skip_ws_and_comments()
            self.expect(":")
            value_name = self.parse_identifier()
            self.skip_ws_and_comments()
            self.expect("}")
            out = TypeRef(category="map", key_name=key_name, value_name=value_name)
        else:
            out = TypeRef(category="simple", name=self.parse_identifier())
        self.skip_ws_and_comments()
        if self.peek() == "[":
            self.expect("[")
            self.expect("]")
            out.category = "array"
        self.skip_ws_and_comments()
        if self.peek() == "?":
            self.expect("?")
            out.optional = True
        elif self.peek() == "!":
            self.expect("!")
            out.explicit_required = True
        return out

    def parse_default(self) -> str:
        self.skip_ws_and_comments()
        if self.peek() == '"':
            return '"' + self.parse_string().replace("\\", "\\\\").replace('"', '\\"') + '"'
        out = []
        while not self.eof():
            ch = self.peek()
            if ch.isspace() or ch == "}":
                break
            out.append(self.get())
        if not out:
            raise ValueError("empty default value")
        return "".join(out)

    def parse_field(self) -> FieldDef:
        self.skip_ws_and_comments()
        field_id = 0
        if self.peek().isdigit():
            field_id = int(self.parse_identifier())
            self.skip_ws_and_comments()
        name = self.parse_identifier()
        self.skip_ws_and_comments()
        self.expect(":")
        type_ref = self.parse_type_ref()
        self.skip_ws_and_comments()
        default = None
        if self.peek() == "=":
            self.expect("=")
            default = self.parse_default()
        return FieldDef(id=field_id, name=name, type_ref=type_ref, default=default)

    def parse_def(self) -> DefBlock:
        name = self.parse_identifier()
        self.skip_ws_and_comments()
        self.expect("{")
        out = DefBlock(name=name)
        while True:
            self.skip_ws_and_comments()
            if self.peek() == "}":
                self.expect("}")
                return out
            out.fields.append(self.parse_field())

    def parse(self) -> SchemaDoc:
        doc = SchemaDoc()
        while True:
            self.skip_ws_and_comments()
            if self.eof():
                break
            keyword = self.parse_identifier()
            if keyword == "schema":
                doc.schema_name = self.parse_identifier()
                continue
            if keyword == "root":
                root_name = self.parse_identifier()
                self.skip_ws_and_comments()
                if self.peek() == "{":
                    self.expect("{")
                    root = DefBlock(name=root_name)
                    while True:
                        self.skip_ws_and_comments()
                        if self.peek() == "}":
                            self.expect("}")
                            break
                        root.fields.append(self.parse_field())
                    doc.root_name = doc.root_name or root.name
                    doc.roots.append(root.name)
                    doc.defs.append(root)
                else:
                    doc.root_name = doc.root_name or root_name
                    doc.roots.append(root_name)
                continue
            if keyword == "import":
                self.skip_ws_and_comments()
                doc.imports.append(self.parse_string())
                continue
            if keyword == "namespace":
                doc.namespace_name = self.parse_identifier()
                continue
            if keyword == "version":
                doc.version = self.parse_integer()
                continue
            if keyword == "hash":
                doc.hash_value = self.parse_integer()
                continue
            if keyword == "allow_unknown":
                value = self.parse_identifier()
                doc.allow_unknown = value in {"true", "T", "1"}
                continue
            if keyword == "def":
                doc.defs.append(self.parse_def())
                continue
            raise ValueError(f"unknown keyword: {keyword}")
        if not doc.schema_name:
            raise ValueError("schema name is empty")
        if not doc.root_name:
            doc.root_name = doc.schema_name.split(".")[-1]
        if not doc.roots:
            doc.roots.append(doc.root_name)
        # 去重但保持声明顺序
        deduped: list[str] = []
        for root in doc.roots:
            if root not in deduped:
                deduped.append(root)
        doc.roots = deduped
        return doc


