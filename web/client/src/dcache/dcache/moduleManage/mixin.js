export  default {
  data () {
    return {
      servers: [],
      transferData: true,
      migrationMethods: [
        { value: true, text: this.$t('dcache.migrationMethod1') },
        { value: false, text: this.$t('dcache.migrationMethod2') }
      ]
    }
  },
  props: {
    expandServers: {
      required: true,
      type: Array
    }
  },
  computed: {
    appName () {
      return this.expandServers[0].app_name
    },
    moduleName () {
      return this.expandServers[0].module_name
    },
    cache_version () {
      return this.expandServers[0].cache_version
    },
    srcGroupName () {
      return this.expandServers[0].group_name
    },
    dstGroupName () {
      return this.servers[0].group_name
    }
  },
  methods: {
    getServers () {
      this.servers = this.newGroup(this.expandServers);
    },
    addNewGroup () {
      const { servers } = this;
      this.servers = servers.concat(this.newGroup(servers.slice(servers.length - this.expandServers.length)));
    },
    deleteGroup () {
      this.servers.splice(this.servers.length - this.expandServers.length, this.expandServers.length);
    },
    newGroup (servers) {
      return servers.map((item, index) => {
        let {group_name, server_name} = item;

        //和tars版本不一样, 不要随便合并
        //  **********name1    => *******name2
        group_name = group_name.replace(/^(.*?)(\d+)$/, function () {
          return arguments[1] + (+arguments[2] + 1)
        });

        // **********name1-1   =>  8888888888name2-1
        server_name = server_name.replace(/^(.*?)(\d+)-\d+$/, function () {
          return arguments[1] + (+arguments[2] + 1) + '-' + (index + 1)
        });

        let server_type = item.server_type;

        return { ...item, group_name, server_name, server_ip: '', shmKey: '', server_type }
      });
    },
  },
}
