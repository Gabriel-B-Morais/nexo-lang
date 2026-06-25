<div align="center">

# Nexo Language

**A modern programming language for unified development**

Backend · Frontend · Desktop · Mobile — one language

![Status](https://img.shields.io/badge/status-in%20development-orange)
![Language](https://img.shields.io/badge/language-C-blue)
![License](https://img.shields.io/badge/license-MIT-green)

</div>

---

## What is Nexo?

Nexo is a modern programming language inspired by PHP, Python, JavaScript, TypeScript and C#, designed to offer a unified experience for backend, frontend, desktop and mobile development.

Write once, run everywhere — with a single language, a single ecosystem, and a single mindset.

## Philosophy

- Simplicity similar to Python
- Optional and progressive typing
- Conventions over configuration
- Integrated ecosystem with official tools
- Backend and frontend using the same language

## Syntax Preview

```nx
// Type inference
name = "Gabriel"
age: int = 30

// Functions — parentheses are optional
func greet name: string -> string
    return "Hello, " + name
end

// Classes with OOP
class User extends Model implements Logger

    use Timestampable

    private id: int
    public name: string

    construct(id: int, name: string)
        this.id = id
        this.name = name
    end

    public func getData() -> object
        return { name: this.name }
    end

end

// Reactive components
page Counter

    state count: int = 0

    action increment()
        count++
    end

    view
        <div>{{count}}</div>
        <button @click="increment">Add</button>
    end

end
```

## Roadmap

### Phase 1 — Core ✅ In Progress
- [x] Lexer
- [x] Parser
- [x] AST
- [ ] Interpreter
- [ ] Type System (strict mode)
- [ ] Namespaces and Imports

### Phase 2 — Ecosystem
- [ ] CLI (`nexo`)
- [ ] Package Manager (`pkm`)
- [ ] Standard Library (`std`)

### Phase 3 — Web
- [ ] Template Engine (`.nxv`)
- [ ] Web Framework (routes, controllers)
- [ ] Native SPA System

### Phase 4 — Platforms
- [ ] Desktop
- [ ] Mobile

## Getting Started

> Nexo is currently in early development. Instructions will be available when the first version is ready.

## Contributing

The project is in its early stages. Feel free to open issues with ideas, suggestions or questions.

## License

MIT License — see [LICENSE](LICENSE) for details.
