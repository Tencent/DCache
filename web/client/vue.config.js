const path = require("path")
const CopyWebpackPlugin = require('copy-webpack-plugin')
const server_port = process.env.SERVER_PORT || '16535'

process.setMaxListeners(0);

// require('events').EventEmitter.setMaxListeners(0);

module.exports = {
  outputDir: "./dist",
  assetsDir: "./static",
  productionSourceMap: false,
  runtimeCompiler: true,
  publicPath: "/plugins/dcache",
  pages: {
    index: {
      entry: 'src/index.js',
      template: 'public/index.html',
      filename: 'index.html',
      title: "DCache",
    },
  },
  configureWebpack: {
    plugins: [new CopyWebpackPlugin([{
      from: path.resolve(__dirname, './static'),
      to: path.resolve(__dirname, './dist/static'),
      ignore: ['.*']
    }])]
  },
  devServer: {
    //是否自动在浏览器中打开
    disableHostCheck: true,
    open: true,
    host: '0.0.0.0',
    //web-dev-server地址
    port: 8188,
    //ajax请求代理
    proxy: {
      "/plugins/*": {
        target: `http://127.0.0.1:${server_port}`,
        changeOrigin: false
      },
    }
  }
}