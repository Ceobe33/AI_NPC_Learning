---
title: Spine Animation Studio
layout: page
date: 2026-09-30 00:00:00
comments: false
# 固定地址：不管这个文件被放到 source/ 的哪一层（甚至被丢进 _posts/），
# 页面都落在 <站点根>/studio/。改这一行就能换地址。
permalink: /studio/
---

在浏览器里查看 Spine 骨骼动画并导出 GIF，全部在本地运行，文件不上传。

<div class="spine-studio-frame">
  <!--
    本页渲染到 /studio/，应用页面在 /spine-studio/，所以用相对路径 ../spine-studio/。
    站点部署在子目录（hexo 的 root: /blog/ 之类）时也照样成立。
    如果你把目录改名了，改这一处 src 即可。
  -->
  <iframe src="../spine-studio/" title="Spine Animation Studio"
          loading="lazy" allowfullscreen></iframe>
</div>

<style>
  .spine-studio-frame iframe {
    display: block;
    width: 100%;
    height: calc(100vh - 220px);
    min-height: 420px;
    border: 1px solid #262b33;
    border-radius: 8px;
    background: #101216;
  }
</style>

用应用里的 **Open File / Open Folder** 选择 Spine 导出文件（骨架 `.json` 或 `.skel`、`.atlas`
与图集图片）。文件只在浏览器里读取，不会上传；导出的 GIF 直接下载到本地。
需要 WebGL 2，2017 年之后的主流浏览器都支持。

[在新窗口打开 →](../spine-studio/)
