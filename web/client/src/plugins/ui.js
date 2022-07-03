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

import Vue from 'vue';
import letUI from 'let-ui';
import 'let-ui/lib/lib.min.css';
import VueCookie from 'vue-cookie';

import '@/assets/css/let-ui.css';
import Icon from '@/components/icon';
import TarsFormItem from '@/components/tars-form-item';
import cn from "let-ui/lib/locale/lang/zh-CN.min";
import en from "let-ui/lib/locale/lang/en-US.min";

import {
  Loading
} from 'element-ui';

Vue.use(letUI, {
  locale: {
    en,
    cn
  } [VueCookie.get('locale') || 'cn'] || cn
});
Vue.component(Icon.name, Icon);
Vue.component(TarsFormItem.name, TarsFormItem);

function UILoading() {
  this.loading = null;
}

UILoading.prototype.show = function (text) {
  if (this.loading) {
    this.hide();
  }

  let loading = Loading.service({
    fullscreen: true,
    text: text || "Loading",
    background: 'rgba(0,0,0,0)',
  });

  this.loading = loading;
  return this;
};

UILoading.prototype.hide = function () {
  if (this.loading) {
    this.loading.close();
    this.loading = null;
  }
  return this;
};

UILoading.show = function (text) {
  if (!UILoading._loading) {
    UILoading._loading = new UILoading();
  }
  return UILoading._loading.show(text);
};

UILoading.hide = function () {
  if (!UILoading._loading) {
    UILoading._loading = new UILoading();
  }
  return UILoading._loading.hide();
};

Object.defineProperty(Vue.prototype, '$Loading', {
  get() {
    if (!this._loading) {
      this._loading = new UILoading();
    }
    return this._loading;
  },
  set(val) {

  }
});

Object.defineProperty(Vue.prototype, '$loading', {
  get() {
    if (!this._loading) {
      this._loading = new UILoading();
    }
    return this._loading;
  },
  set(val) {

  }
});

//////////////////////////////////////////////////////////////////////////////////////

let tip = {
  error: (msg) => {
    Vue.prototype.$message({
      showClose: true,
      message: msg,
      type: "error",
    });
  },
  success: (msg) => {
    Vue.prototype.$message({
      showClose: true,
      message: msg,
      type: "success",
    });
  },
  warning: (msg) => {
    Vue.prototype.$message({
      showClose: true,
      message: msg,
      type: "warning",
    });
  }
}
Object.defineProperty(Vue.prototype, '$tip', {
  get() {
    return tip;
  },
  set(val) {

  }
});