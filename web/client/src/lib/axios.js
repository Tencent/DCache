import Axios from 'axios'
import qs from 'qs'

const axios = Axios.create({
  baseURL: '/plugins/dcache/server/api',
  timeout: 25000,
  // headers: { 'content-type': 'application/json;charset=UTF-8' },
  transformRequest: [function (data, headers) {

    // 上传数据不序列化
    if (headers['Content-Type'] === 'multipart/form-data') {
      return data
    }
    if (headers['Content-Type'] === 'application/json') {
      return JSON.stringify(data)
    }
    return qs.stringify(data);
  }]
});

axios.interceptors.request.use((config) => {

  return config
}, err => {

  throw new Error(err)
});

axios.interceptors.response.use((response) => {
  let {
    data
  } = response;

  if (data.ret_code === 200) {
    return data.data;
  } else {
    throw new Error(data.err_msg)
  }

}, err => {

  throw new Error(err)
});

export default axios