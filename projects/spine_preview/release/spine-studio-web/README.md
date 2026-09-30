# 把网页版打进任意 Hexo

`web/source` 是一个可移植页面模板，`scripts/package-web.sh` 把它和编译好的
WebAssembly 合成一个目录，直接拷进任何 Hexo 站的 `source/` 就能用。
**不依赖主题 layout**，换主题、换站点都不影响。

## 三步

```bash
# 1. 编译（产物默认落在 ../../site/source/spine-studio）
EMSDK=~/emsdk ./scripts/build-wasm.sh

# 2. 打包 -> release/spine-studio-web/
./scripts/package-web.sh
#   或者直接装进目标博客：
#   ./scripts/package-web.sh --install ~/my-blog/source

# 3. 生成
cd ~/my-blog && hexo clean && hexo generate
```

然后访问 `/spine-studio/`（整页）或 `/studio/`（套在博客主题里）。

## 目录结构

拷贝 `release/spine-studio-web/source/` 下这两个目录到目标博客的 `source/`：

```
source/
  spine-studio/
    index.html                   整页入口 -> /spine-studio/
    spine-animation-studio.js
    spine-animation-studio.wasm
  studio/
    index.md                     md 页面 + iframe -> /studio/
```

两个入口任选，也可以都留着：前者全屏、不受主题干扰；后者保留博客导航和页脚，
应用在 iframe 里跑，主题的 CSS / PJAX / 懒加载都碰不到它。

注意拷的是 **`source/` 里的那两个目录**，不是 `web/` 目录本身（它只是仓库里的
模板，`web/` 下面还套了一层 `source/`，整拷过去地址就会多出几级）。

## 放错位置会怎样

典型症状：`Cannot GET /2026/09/30/...`、`Cannot GET /.../web/source/studio/...`。
两种原因，都是位置问题：

| 放到了 | 结果 |
| --- | --- |
| `source/_post/…`（单数） | Hexo 把下划线开头的路径当隐藏文件**整个跳过**，什么都不会生成 |
| `source/_posts/…`（复数） | 变成文章，地址是 `:year/:month/:day/:title/`，相对路径也跟着错位 |
| `source/web/source/…` | 地址多出几级，iframe 的相对路径指不到应用 |

修法：把误放的目录删掉，按上面的结构放回 `source/` 根，然后
`hexo clean && hexo generate`（`hexo server` 要重启，它有缓存）。

两个入口文件都带了 `permalink`，所以页面地址是固定的，不会变成
`/2026/09/30/...`。但这只是让地址好看：**`_posts/` 里的 `.js` / `.wasm` 会被当
文章处理、不会作为静态资源发布**，应用照样跑不起来。

顺带一提，别把 `web/README.md` 也拷进去：它没有 front matter，Hexo 解析时会报
`YAMLException: end of the stream ...`。

结论：想让它工作，位置只能是 `source/` 根。

## 为什么这样就能移植

Hexo 有个容易踩的坑：**它连 `.html` 都会渲染**。核心自带 `html → html` 的
渲染器，所以 `source/` 下的 `.html` 会被当成页面、套上主题的 layout —— 页面
就毁了。`spine-studio/index.html` 顶部那三行 front matter 就是解药：

```
---
layout: false
---
```

`layout: false` 时 Hexo 直接把文件内容作为结果输出（生成时 front matter 会被
剥掉），页面因此不依赖任何主题，也不用去改博客的 `_config.yml`。

两条可选的等价做法，看你的偏好：

- 在博客 `_config.yml` 加 `skip_render: spine-studio/index.html`，然后删掉
  文件顶部的 front matter；
- 或者干脆只用 `/studio/` 那个 md + iframe 入口，它本来就是正常 md 页面。

其余要点：

- `.js`、`.wasm` 没有渲染器，Hexo 原样复制，字节不变。
- Emscripten 用 `document.currentScript.src` 推导 wasm 路径，只要 `.js` 和
  `.wasm` 在同一个目录，页面放在站点哪一层都能跑，子目录部署也没问题。
- 不要为了"省一个请求"把 `.js` 内联进 HTML：那样 `currentScript` 为空，
  wasm 就找不到了。
- 源文件直接拖进浏览器会看到顶部三行 `---`（Hexo 之外没人剥它）。要放到
  非 Hexo 的静态托管，删掉这三行即可。

## 注意

- **wasm 的 MIME 必须是 `application/wasm`**。GitHub Pages、Vercel、Netlify
  默认正确；自建 nginx 需要 `types { application/wasm wasm; }`，
  `hexo server` 也没问题。
- 需要 **WebGL 2**，2017 年之后的主流浏览器都支持。
- 模块约 1.6 MB，进 git 会让博客仓库变大。不想常驻仓库的话，用 CI 在构建时
  生成（`WASM_OUTPUT=... ./scripts/build-wasm.sh` 可以指定任意输出目录），
  或者让 GitHub Actions 把产物推到博客仓库。
- 入口改成别的名字可以，但 `#spine-studio` 和 `#spine-studio-canvas` 这两个
  id 不能动：`src/main.cpp` 里的 `kCanvasHostSelector` / `kCanvasSelector`
  直接按 id 查。
