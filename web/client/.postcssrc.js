// https://github.com/michael-ciniawsky/postcss-load-config

module.exports = {
  "plugins": {
    "postcss-import": {},
    "postcss-url": {},
    "postcss-nested": {},
    // to edit target browsers: use "browserslist" field in package.json
    "postcss-cssnext": {
      features: {
        autoprefixer: {
          browsers: [
            'Android >= 4',
            'Chrome >= 35',
            'Firefox >= 31',
            'Explorer >= 9',
            'iOS >= 7',
            'Opera >= 12',
            'Safari >= 7.1',
          ],
        }
      }
    }
  }
}
