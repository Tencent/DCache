/**
 * Tencent is pleased to support the open source community by making Tars available.
 *
 * Copyright (C) 2016THL A29 Limited, a Tencent company. All rights reserved.
 *
 * Licensed under the BSD 3-Clause License (the "License"); you may not use this file except 
 * in compliance with the License. You may obtain a copy of the License at
 *
 * https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing, software distributed 
 * under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR 
 * CONDITIONS OF ANY KIND, either express or implied. See the License for the 
 * specific language governing permissions and limitations under the License.
 */

import Vue from "vue";
import VueI18n from "vue-i18n";
import VueCookie from "vue-cookie";
import Ajax from '@/plugins/ajax';
import locale_zh from 'element-ui/lib/locale/lang/zh-CN';
import locale_en from 'element-ui/lib/locale/lang/en';

Vue.use(VueI18n);
Vue.use(VueCookie);

export const i18n = new VueI18n({});
i18n.setLocaleMessage('cn', locale_zh);
i18n.setLocaleMessage('en', locale_en);

export var localeMessages = [];

export function loadLang() {
  return new Promise((resolve, reject) => {
    Ajax.getJSON('/get_locale').then((locales) => {
      let locale = VueCookie.get('locale');
      if (Object.prototype.toString.call(locales) == '[object Object]') {
        for (var localeCode in locales) {
          i18n.mergeLocaleMessage(localeCode, locales[localeCode]);
          localeMessages.push({
            localeCode: localeCode,
            localeName: locales[localeCode]['localeName'],
            localeMessages: locales
          })
        }
        locale = locales[locale] ? locale : 'cn';
        localeMessages = locales;
      }
      i18n.locale = locale;
      resolve();
    }).catch((err) => {
      reject(err);
    });
  })
};