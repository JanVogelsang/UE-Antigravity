<p align="center">
  <h1 align="center">🚀 UE-AgentFramework</h1>
  <p align="center">
    <strong>The most advanced open-source AI agent integration for Unreal Engine — and it's completely free.</strong>
  </p>
  <p align="center">
    Give your favorite AI coding agents (Antigravity, Claude Code, OpenAI Codex, Kilo Code…) deep, autonomous, native control over your Unreal Engine project — without subscriptions, credits, or your source code ever leaving your machine.
  </p>
</p>

<p align="center">
  <img alt="License: MIT" src="https://img.shields.io/badge/License-MIT-green.svg">
  <img alt="Price: Free forever" src="https://img.shields.io/badge/Price-Free%20forever-brightgreen.svg">
  <img alt="Unreal Engine 5.7 | 5.8" src="https://img.shields.io/badge/Unreal%20Engine-5.7%20%7C%205.8-blue.svg">
  <img alt="Bring your own agent" src="https://img.shields.io/badge/Agent-Bring%20your%20own-orange.svg">
  <img alt="PRs welcome" src="https://img.shields.io/badge/PRs-welcome-blueviolet.svg">
</p>

<p align="center">
  <a href="#-why-ue-agentframework">Why Us</a> •
  <a href="#-how-were-different">How We're Different</a> •
  <a href="#-architecture">Architecture</a> •
  <a href="#-supported-unreal-engine-features">Supported Features</a> •
  <a href="#-quick-start--installation">Install</a> •
  <a href="#-supported-unreal-engine-versions">Versions</a>
</p>

---

## 🌟 Why UE-AgentFramework?

The market for Unreal Engine AI assistants is filling up fast — but almost all of them share the same three problems: they're **closed-source**, they **charge you per action** through subscriptions and metered credits, and they lock the real intelligence inside a **hand-rolled chat window** bolted into the editor.

**UE-AgentFramework rejects all three.** It is **fully open-source (MIT)**, **free forever**, and built on a **Dual-MCP + C++ AST architecture** that hands the world's best AI agents deep, native, autonomous access to the live Unreal Editor — while your code stays on your machine.

Instead of reinventing an agent loop, we plug directly into the purpose-built agent harnesses that already outperform anything a plugin vendor could hand-craft — **Antigravity, Claude Code, OpenAI Codex, and Kilo Code** — and expose the engine to them through a clean, token-efficient tool layer.

### 🔥 What sets us apart

- **🆓 Truly free & fully open-source.** MIT-licensed. No subscription, no credit meter that drains on every prompt, no vendor cloud you're forced to route your project through. Audit it, fork it, extend it.
- **🧠 Real code intelligence, not text search.** A dedicated **Clang-powered C++ AST server** resolves class hierarchies, method signatures, property types, and transitive call graphs across your whole codebase. Competitors read files; we *understand* them.
- **🔎 Semantic project understanding.** Auto-generated project index, **Blueprint semantic vector search** (find "the Blueprint that handles player health" by meaning, not name), and semantic search over UE documentation.
- **⚡ Engineered for token efficiency.** Every tool response is compacted — connection reports, Blueprint readbacks, and schemas are compressed and hashed so your agent spends its context solving problems, not parsing bloat. Lower cost, higher-quality results.
- **🤖 Bring your own agent.** We ride the battle-tested reasoning, context management, and self-correction of the best agent platforms — and every upgrade they ship makes your UE workflow better for free.
- **🛠️ 120+ native engine tools across every major subsystem** — Blueprints, C++, Niagara, PCG, Materials, UMG, Animation, GAS, Behavior Trees, Sequencer, Enhanced Input, Replication, Profiling, autonomous PIE playtesting, packaging, and more.
- **🔒 Private by default.** No mandatory cloud round-trip, no "training on by default," no NDA headaches. Run entirely against local or self-hosted models if you choose.

---

## ⚖️ How We're Different

Most competitors fall into two camps. **Closed cloud copilots** give you a polished chat box but charge metered credits and send your project to their servers. **Other open plugins** are free but hand-roll their own agent loop and understand your code only at the file level. We took the best of both and went deeper.

| | **UE-AgentFramework** | Closed cloud copilots | Other open plugins |
|---|:---:|:---:|:---:|
| **Price** | ✅ Free forever | ❌ Subscription + per-action credits | ✅ Free (bring your own key) |
| **Source** | ✅ Fully open (MIT) | ❌ Closed / source-available | 🟡 Varies |
| **Agent harness** | ✅ Bring your own (Claude Code, Antigravity, Codex, Kilo) | ❌ Proprietary in-editor chat | ❌ Hand-rolled in-editor loop |
| **C++ understanding** | ✅ Clang AST + call graphs + UHT reflection | 🟡 Project scan / file read | ❌ File-level only |
| **Semantic search** | ✅ Blueprint vectors + UE-docs search | ❌ | ❌ |
| **MCP interoperability** | ✅ Native Dual-MCP surface | 🟡 Partial / docs-only | ❌ None |
| **Token optimization** | ✅ Compacted, hashed, cached | 🟡 Varies | 🟡 Varies |
| **Data privacy** | ✅ Local — no forced cloud round-trip | ❌ Cloud required | ✅ Local |
| **Model choice** | ✅ Any model, any provider | ❌ Vendor-locked / undisclosed | ✅ Any key |

> We don't compete on raw tool-count marketing. We compete on **architecture, code intelligence, and cost** — the things that actually determine whether an agent ships correct work.

---

## 🏛️ Architecture

Most UE AI tools embed a basic LLM chat window inside the editor with a hand-crafted agent loop. **UE-AgentFramework inverts this model** — we expose the engine to the world's best AI agents through a standardized protocol and let each agent's proven reasoning do the heavy lifting.

### Dual-MCP + AST Server

```
┌─────────────────────────────────┐     ┌──────────────────────────────────┐
│  Your AI Agent                  │     │  Unreal Engine Editor            │
│  (Antigravity / Claude Code /   │◄───►│                                  │
│   Codex / Kilo / any MCP client)│ MCP │  ┌────────────────────────────┐  │
│                                 │     │  │ AgentFramework C++ Plugin  │  │
│  ┌───────────────────────────┐  │     │  │ Internal MCP Server :18777 │  │
│  │ Agent Skills & Rules      │  │     │  │ 120+ Game-Thread Tools     │  │
│  │ System Prompt & Profiles  │  │     │  └────────────────────────────┘  │
│  └───────────────────────────┘  │     │                                  │
│                                 │     └──────────────────────────────────┘
│  ┌───────────────────────────┐  │
│  │ External Python AST       │  │
│  │ Server & Bridge Proxy     │  │
│  │ • C++ Symbol Resolution   │  │
│  │ • Call-Graph Analysis     │  │
│  │ • Semantic Vector Search  │  │
│  │ • Blueprint / Docs Index  │  │
│  └───────────────────────────┘  │
└─────────────────────────────────┘
```

- **Internal C++ MCP Server** — an in-editor HTTP loopback server (port `18777`) that executes Game-Thread operations in real time: reading Blueprints, injecting T3D node graphs, managing components, compiling, profiling, driving PIE, and more.
- **External Python AST Server** — parses your project's C++ via Clang to resolve class hierarchies, method signatures, property types, and transitive call graphs, plus semantic vector search over Blueprints and UE docs. This is the layer that separates *understanding* your project from *grepping* it.
- **MCP Bridge Proxy** — translates MCP JSON-RPC between the agent platform and both servers, with tool-profile support and schema caching.

### Why bring-your-own-agent beats an in-editor chat UI

Every closed competitor rebuilds an LLM chat interface from scratch inside the editor — simplistic single-turn reasoning, fragile context management, limited error recovery, and enormous engineering effort spent on UI instead of engine integration. By leveraging **purpose-built agent platforms**, your agent instead gets:

- **Superior multi-step autonomous reasoning** with self-correction loops
- **Battle-tested context management** (condensation, truncation, caching)
- **A rich surrounding tool ecosystem** (file editing, web search, terminal, git) alongside our UE tools
- **Continuous, free improvements** — every platform upgrade makes your UE workflow better without us shipping a thing

### Deep project understanding

Before touching a line of code, agents genuinely *understand* your project:

- **Automated Project Indexing** — the `unreal-setup` skill scans your project and generates a token-efficient index of all C++ classes, Blueprints, assets, gameplay systems, and architectural patterns, stored as a compact skill loaded every session.
- **C++ AST queries** — class hierarchies, declarations, property types, and call graphs across the entire codebase.
- **Blueprint semantic search** — find Blueprints by *description*, not just asset name.
- **UE documentation search** — semantic lookup ensures agents reference correct API usage.

---

## 🛠️ Supported Unreal Engine Features

UE-AgentFramework ships **120+ specialized MCP tools** and **12 expert-crafted skills**, each engineered for minimal token usage and maximum reliability. Here's the full coverage matrix.

### Coverage at a glance

| Subsystem | Support | What agents can do |
|---|:---:|---|
| **Blueprints** | ✅ Deep | T3D node-graph injection, components (SCS), variables/functions/events, pin wiring, defaults, batch transactions, connection verification, graph analysis, schema introspection, compile diagnostics |
| **C++ Development** | ✅ Deep | Create/modify classes with header+source scaffolding, Live Coding / Hot Reload, regenerate project files, UHT reflection queries — plus the **Clang AST server** for symbol resolution & call-graph analysis |
| **Niagara VFX** | ✅ | Create systems & emitters, insert & configure script modules and curves, compile, capture temporal keyframe preview grids for visual QA |
| **Procedural Content (PCG)** | ✅ | Create graphs, attach components, configure parameters, run local generation, introspect graph structure |
| **Materials** | ✅ | Create Materials & Instances, add expression nodes, wire outputs to material properties, capture previews |
| **UMG / Widget UI** | ✅ | Build widget hierarchies (30+ widget types), configure slots/anchors/alignment, set properties, fonts & brushes, bind events, compile, capture, instantiate full UIs from JSON |
| **Animation** | ✅ | Create Animation Blueprints, import FBX animations, assign AnimBPs, create Montages, query skeleton compatibility |
| **Gameplay Ability System (GAS)** | ✅ | Register Gameplay Tags, create Attribute Sets, configure ASCs, create Gameplay Effects & Abilities |
| **Behavior Trees & AI** | ✅ | Create Blackboards & Behavior Trees, inject BT node graphs, configure Navigation Mesh |
| **Enhanced Input** | ✅ | Create Input Actions & Mapping Contexts, add key/button mappings, full C++/Blueprint binding via skill |
| **Sequencer & Cinematics** | ✅ | Create Level Sequences, add tracks (transform, animation, camera…), add keyframes |
| **Data Tables & Data Assets** | ✅ | Create Data Tables & Assets, import JSON into Data Tables, set & introspect Data Asset properties |
| **Network Replication** | ✅ | Guided setup for replicated properties, RepNotify, Server/Client RPCs, DOREPLIFETIME lifetime conditions |
| **Level Editing** | ✅ | Spawn actors by class/Blueprint with transforms, place lights (Point/Spot/Directional/Rect), modify World Settings |
| **Performance & Profiling** | ✅ Deep | Memory & frame-timing stats, stat commands, asset-size analysis, CSV profiler (start/stop/read), CVar get/set/discover, arbitrary console commands, scalability & renderer settings |
| **Play-In-Editor Automation** | ✅ Deep | Start/stop PIE, simulate keyboard & gamepad input, extract full UMG/Slate widget trees as JSON, trigger UI elements, query world actors by class/tag/radius — fully autonomous playtesting |
| **Mesh & Asset Import** | ✅ | Import FBX/OBJ as static/skeletal, batch-import (FBX, OBJ, PNG, TGA, WAV, MP3), configure Nanite/LODs/collision/lightmaps |
| **Build & Packaging** | ✅ | Build lighting (Preview→Production), package for Win64/Linux/Mac/Android/iOS |
| **Source Control** | ✅ | Checkout, add, revert, status for Perforce or Git |
| **Validation & Testing** | ✅ | Validate assets for cook/package errors, run UE Automation Tests by filter, testing SOPs for PIE/perf/E2E |
| **Python & Diagnostics** | ✅ | Execute Python in the editor environment, capture viewport screenshots, read the output/message log, control editor state |

### The 12 expert-crafted skills

Skills are structured, token-efficient playbooks the agent loads on demand:

| Skill | Purpose |
|---|---|
| `unreal-setup` | One-time project scan → generates the machine-specific env + a permanent `project-index` skill |
| `unreal-instructions` | Master routing guide for the Dual-MCP ecosystem and tool selection |
| `blueprint-authoring` | T3D rules, layout formatting, and SOPs for creating/modifying Blueprint graphs |
| `niagara-authoring` | Create, modify, compile, and visually evaluate AAA-quality Niagara systems |
| `create-actor` | Scaffold a complete Actor with C++ class and optional Blueprint child |
| `create-interface` | Create a UInterface with C++ backing for interactable objects |
| `add-component` | Add and configure a component on an existing actor class |
| `setup-input` | Configure Enhanced Input with Actions and Mapping Contexts |
| `setup-replication` | Configure an actor for multiplayer network replication |
| `pie-verifier` | Autonomously run PIE, navigate UI, click elements, and query world state |
| `unreal-testing-sops` | Minimal-token SOPs for reliably testing game functionality |
| `python-env` | Python environment, test execution, and IPC troubleshooting |

---

## 🎮 Supported Unreal Engine Versions

- **Unreal Engine 5.8 (Recommended)** — Exceptional support; fully integrates and extends the official Epic Games UE MCP server.
- **Unreal Engine 5.7** — Tested and fully supported.
- **Other UE 5.x versions** — Likely to work, but not actively tested.

---

## 🚀 Quick Start & Installation

Setup is painless. An automated PowerShell installer handles file copying, Python virtual environments, dependency installation, and configuration for your preferred assistant.

### 1. Install

1. **Close the Unreal Editor** so plugin files can be copied safely.
2. **Run the installer** from this repository's root:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\UnrealEngine\install.ps1
   ```
   *(If it fails due to Windows security policy, run it from a terminal rather than double-clicking.)*
3. **Follow the prompts** — enter the absolute path to your target UE project and pick your AI assistant.
4. **Launch the Editor** and choose **Yes** if prompted to compile missing modules for the `AgentFramework` plugin. This starts the internal loopback server.

### 2. Initialize your AI workspace

Open your AI assistant (Antigravity, Claude Code, OpenAI Codex, or Kilo) in your target UE project folder and send:

> `"Run the unreal-setup skill to configure the project"`

The agent scans your C++ codebase, configures the local environment, and builds a token-efficient project index. **Restart your assistant** afterward to load the newly generated skills.

---

## 🏗️ Repository Structure

- **`AgentFramework/`** — the Unreal Engine C++ editor plugin. Runs the internal MCP server with 120+ Game-Thread tools.
- **`UnrealEngine/`** — the agent plugin: MCP bridge proxy, Python AST server, 12 skills, system prompts, and agent profiles.
- **`Tests/`** — a `pytest`-based integration and unit suite verifying the Dual-MCP communication flow.
- **`Documentation/`** — architecture, developer setup, and testing-matrix docs.

---

## 🌱 Lineage & Acknowledgments

**UE-AgentFramework is a proud fork and extension of the open-source [Autonomix](https://github.com/PRQELT/Autonomix) plugin by QXMP Labs.** 

We built upon their robust, native C++ in-editor tool foundation (which pioneered T3D injection and execution validation) and extended it to support external agent workflows. Specifically, we added:
- The **External Python Clang C++ AST Server** for true semantic code intelligence.
- **Vector databases** for Blueprint and UE documentation semantic search.
- The **MCP Bridge Proxy** to detach the agent loop from the editor, enabling the "Bring Your Own Agent" model (Antigravity, Claude Code, etc.).
- The **12-Skill System** for token-efficient playbook execution.

We are deeply grateful to the Autonomix maintainers for their incredible foundation.

---

## 🤝 Contributing

UE-AgentFramework is open-source and community-driven. Issues, feature requests, and pull requests are welcome — whether it's a new engine subsystem, a token-efficiency improvement, or better support for another agent platform.

---

## 📄 License

Released under the **MIT License**. See the [`LICENSE`](LICENSE) file for details.
