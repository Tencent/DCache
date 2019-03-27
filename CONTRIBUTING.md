# Contributing

我们提倡您通过提 issue 和 pull request 方式来促进 DCache 的发展。

即使是在 issue 中回答问题，或者做一些简单的 bugfix ，也会给 Dcache 带来很大的帮助。

## Issue 提交

#### 对于贡献者

在提 issue 前请确保满足一下条件：

- 必须是一个 bug 或者功能新增。
- 已经在 issue 中搜索过，并且没有找到相似的 issue 或者解决方案。
- 完善下面模板中的信息

如果已经满足以上条件，我们提供了 issue 的标准模版，请按照模板填写。

​             

##  Pull request

我们除了希望听到您的反馈和建议外，我们也希望您接受代码形式的直接帮助，对我们的 GitHub 发出 pull request 请求。

以下是具体步骤：

#### Fork仓库

点击 `Fork` 按钮，将需要参与的项目仓库 fork 到自己的 Github 中。

#### Clone 已 fork 项目

在自己的 github 中，找到 fork 下来的项目，git clone 到本地。

```bash
$ git clone git@github.com:<yourname>/DCache.git
```

#### 添加 DCache 仓库

将 fork 源仓库连接到本地仓库：

```bash
$ git remote add <name> <url>
# 例如：
$ git remote add DCache git@github.com:Tencent/DCache.git
```

#### 保持与 DCache 仓库的同步

更新上游仓库：

```bash
$ git pull --rebase <name> <branch>
# 等同于以下两条命令
$ git fetch <name> <branch>
$ git rebase <name>/<branch>
```

#### commit 信息提交

commit 信息请遵循[commit消息约定](./CONTRIBUTING_COMMIT.md)，具体格式请参考 commit 文档规范。
