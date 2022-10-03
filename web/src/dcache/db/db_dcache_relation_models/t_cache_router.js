const Sequelize = require('sequelize');
module.exports = function(sequelize, DataTypes) {
  return sequelize.define('t_cache_router', {
    id: {
      autoIncrement: true,
      type: DataTypes.INTEGER,
      allowNull: false,
      primaryKey: true
    },
    cache_name: {
      type: DataTypes.STRING(128),
      allowNull: false,
      defaultValue: "",
      unique: "cache_name"
    },
    cache_ip: {
      type: DataTypes.STRING(30),
      allowNull: false,
      defaultValue: ""
    },
    router_name: {
      type: DataTypes.STRING(128),
      allowNull: false,
      defaultValue: ""
    },
    db_name: {
      type: DataTypes.STRING(64),
      allowNull: false,
      defaultValue: ""
    },
    db_ip: {
      type: DataTypes.STRING(64),
      allowNull: false,
      defaultValue: ""
    },
    db_port: {
      type: DataTypes.STRING(8),
      allowNull: false,
      defaultValue: ""
    },
    db_user: {
      type: DataTypes.STRING(64),
      allowNull: false,
      defaultValue: ""
    },
    db_pwd: {
      type: DataTypes.STRING(64),
      allowNull: false,
      defaultValue: ""
    },
    db_charset: {
      type: DataTypes.STRING(8),
      allowNull: false,
      defaultValue: ""
    },
    module_name: {
      type: DataTypes.STRING(64),
      allowNull: false,
      defaultValue: ""
    },
    app_name: {
      type: DataTypes.STRING(80),
      allowNull: false,
      defaultValue: ""
    },
    idc_area: {
      type: DataTypes.STRING(10),
      allowNull: false,
      defaultValue: ""
    },
    group_name: {
      type: DataTypes.STRING(128),
      allowNull: false,
      defaultValue: ""
    },
    server_status: {
      type: DataTypes.CHAR(1),
      allowNull: false,
      defaultValue: ""
    },
    priority: {
      type: DataTypes.TINYINT,
      allowNull: false,
      defaultValue: 1
    },
    modify_time: {
      type: DataTypes.DATE,
      allowNull: false,
      defaultValue: Sequelize.Sequelize.literal('CURRENT_TIMESTAMP')
    },
    mem_size: {
      type: DataTypes.INTEGER,
      allowNull: true
    }
  }, {
    sequelize,
    tableName: 't_cache_router',
    timestamps: false,
    indexes: [
      {
        name: "PRIMARY",
        unique: true,
        using: "BTREE",
        fields: [
          { name: "id" },
        ]
      },
      {
        name: "cache_name",
        unique: true,
        using: "BTREE",
        fields: [
          { name: "cache_name" },
        ]
      },
      {
        name: "app_name",
        using: "BTREE",
        fields: [
          { name: "app_name" },
        ]
      },
      {
        name: "module_name",
        using: "BTREE",
        fields: [
          { name: "module_name" },
        ]
      },
      {
        name: "cache_ip",
        using: "BTREE",
        fields: [
          { name: "cache_ip" },
        ]
      },
      {
        name: "group_name",
        using: "BTREE",
        fields: [
          { name: "group_name" },
        ]
      },
      {
        name: "server_status",
        using: "BTREE",
        fields: [
          { name: "server_status" },
        ]
      },
      {
        name: "idc_area",
        using: "BTREE",
        fields: [
          { name: "idc_area" },
        ]
      },
    ]
  });
};
