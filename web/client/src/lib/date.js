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

/* eslint-disable prefer-template, no-nested-ternary */
export const ONE_SECOND = 1000;
export const ONE_MINUTE = 60 * ONE_SECOND;
export const ONE_HOUR = 60 * ONE_MINUTE;
export const ONE_DAY = 24 * ONE_HOUR;
export const ONE_MONTH = 30 * ONE_DAY;
export const ONE_YEAR = 365 * ONE_DAY;

export function toDate(date) {
  return date == null ? new Date() : (date instanceof Date ? date : new Date(date));
}

export function formatDate(date, format) {
  date = toDate(date);
  format = format || 'YYYY-MM-DD HH:mm:ss';

  // 非正常的日期
  if (isNaN(date.getTime())) {
    return format;
  }

  const mappings = {
    'M+': date.getMonth() + 1,
    'D+': date.getDate(),
    'H+': date.getHours(),
    'h+': date.getHours() % 12 === 0 ? 12 : date.getHours() % 12,
    'm+': date.getMinutes(),
    's+': date.getSeconds(),
    'q+': Math.floor((date.getMonth() + 3) / 3),
    S: date.getMilliseconds(),
  };

  if (/(Y+)/.test(format)) {
    format = format.replace(RegExp.$1, (date.getFullYear() + '').substr(4 - RegExp.$1.length));
  }

  if (/(dd+)/.test(format)) {
    format = format.replace(RegExp.$1, '日一二三四五六七'.split('')[date.getDay()]);
  }

  Object.keys(mappings).forEach((key) => {
    if (new RegExp('(' + key + ')').test(format)) {
      const matchedVal = RegExp.$1;
      const val = `${mappings[key]}`;
      format = format.replace(matchedVal, matchedVal.length === 1 ? val : `00${val}`.substr(val.length));
    }
  });

  return format;
}

export function toNow(date) {
  const time = toDate(date).getTime();

  // 非正常的日期
  if (isNaN(time)) {
    return date;
  }

  const duration = new Date().getTime() - time;

  if (duration > ONE_YEAR) {
    return '1年前';
  }
  if (duration > ONE_MONTH) {
    return Math.floor(duration / ONE_MONTH) + '个月前';
  }
  if (duration > ONE_DAY) {
    return Math.floor(duration / ONE_DAY) + '天前';
  }
  if (duration > ONE_HOUR) {
    return Math.floor(duration / ONE_HOUR) + '小时前';
  }
  if (duration > ONE_MINUTE) {
    return Math.floor(duration / ONE_MINUTE) + '分钟前';
  }
  if (duration > 0) {
    return '刚刚';
  }
  return formatDate(date);
}
