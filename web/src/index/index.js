const LocaleController = require('./controller/locale/LocaleController');
const PageController = require('./controller/page/PageController');
const localeApiConf = [
    //语言包接口
    ['get', '/get_locale', LocaleController.getLocale]
];

const pageApiConf = [
    //首页
    ['get', '/', PageController.index],
];

module.exports = {
    pageApiConf,
    localeApiConf,
};