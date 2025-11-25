# Contributing to FidoNext

Thank you for your interest in contributing to **FidoNext** — a decentralized, privacy‑first, community‑owned communication protocol.

FidoNext is fully open-source, and we welcome contributions from developers, designers, researchers, cryptographers, protocol engineers, and privacy enthusiasts.

---

## 🧭 How to Contribute

### 1. Fork the repository
Click “Fork” on GitHub and clone your fork:

```
git clone https://github.com/your-username/fidonext.git
```

### 2. Create a feature branch
```
git checkout -b feature/my-new-feature
```

### 3. Follow the coding guidelines
- Rust code should follow `rustfmt` standards.
- Keep functions small, modular, and well-documented.
- Cryptographic code must follow formally recognized standards.
- Write clear commit messages.

### 4. Add tests
All new features must include appropriate tests:
- Unit tests  
- Integration tests (where needed)

### 5. Submit a Pull Request
Push your branch and create a PR:

```
git push origin feature/my-new-feature
```

Include:
- A clear description of the change
- Motivation / context  
- Tests included  
- Any breaking changes

---

## 🧱 Development Areas

We especially welcome contributions in:

### 🔹 Networking (Rust)
- libp2p integrations  
- Relay systems  
- NAT traversal improvements  
- DHT optimizations  

### 🔹 Cryptography
- Secure key storage  
- Protocol verification  
- Signal-style encrypted channels  
- Metadata minimization  

### 🔹 Protocol Research
- Routing  
- Overlay networks  
- Censorship resistance  
- Distributed identity  

### 🔹 Mobile Development
- Kotlin + Rust FFI  
- Swift + Rust bindings  

### 🔹 UI/UX
- Privacy-first interface design  
- Community channels  
- Multi-peering flows  

---

## 🧪 Testing

Run all tests:

```
cargo test
```

Run lints:

```
cargo clippy
```

Format code:

```
cargo fmt
```

---

## 📄 Documentation

Please document:
- All public functions
- All protocol flows
- All components in `/docs`
- All new architectural decisions (ADR format is welcome)

---

## 🛡 Security Reporting

For **security issues**, do NOT create a public issue.

Email:

**security@fidonext.org**

Provide:
- Detailed description  
- Reproduction steps  
- Potential impact  

---

## 🙌 Thank You

Your contributions help build a censorship-resistant, private, community‑owned communication network.  
We are grateful for every PR, issue, idea, and bug report.

Welcome to the movement.
