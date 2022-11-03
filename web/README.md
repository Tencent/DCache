# 如何开发

由于dcache web是作为插件挂载tarsweb后的, 它是无法本地调试的, 如果要调试需要:准备以下几点:

- 要有一套tars环境, 并安装好dcache组件
- tarsweb上可以打开缓存平台
- 在tarsweb上将dcacheweb的端口修改为: 8188
- 开终端, 进入`/usr/local/app/tars/tarsnode/data/DCache.DCacheWebServer/bin/client`, 执行 npm install, 运行: export SERVER_HOST=${your host}; npm run dev
- 开新终端, 进入`/usr/local/app/tars/tarsnode/data/DCache.DCacheWebServer/bin` 目录, 执行 npm install, 运行: npm run local
- 可能需要重启tarsweb(pm2 restart tars-node-web)
- 通过浏览器打开tarsweb, 进入缓存平台
- 至此, web已经进入了开发模式, 你调整代码, 马上会生效