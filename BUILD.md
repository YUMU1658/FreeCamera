# Build (26.10)

```powershell
xmake repo -u
xmake f -a x64 -m release -p windows -y
xmake -y
```

Output: `bin/FreeCamera/`

## Toolchain pins (`xmake.lua`)

| Package | Version |
|---------|---------|
| levilamina | **26.10.0** (see note below) |
| gmlib | 26.10.* |
| levibuildscript | 0.6.1 |

**Note:** GMLIB **26.10.0** `SDK-static` is built against LeviLamina **26.10.0** bedrock symbols. Linking with **levilamina 26.10.14** may fail (`Block::tryGetFromRegistry` / `GMLevel`). Run the server with any **26.10.x** LeviLamina + matching GMLIB runtime; rebuild this plugin when GMLIB-Release ships an updated SDK-static for newer 26.10 loaders.

Optional workaround script (removes `Level.cpp.obj` only — still insufficient if `GMPlayer` needs `GMLevel::getInstance`): `scripts/patch-gmlib-static.ps1`