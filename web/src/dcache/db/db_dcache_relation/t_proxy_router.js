const Sequelize = require('sequelize');
module.exports = function(sequelize, DataTypes) {
  return sequelize.define('t_proxy_router', {
    id: {
      autoIncrement: true,
      type: DataTypes.INTEGER,
      allowNull: false,
      primaryKey: true
    },
    proxy_name: {
      type: DataTypes.STRING(128),
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
    modulename_list: {
      type: DataTypes.TEXT,
      allowNull: false
    },
    modify_time: {
      type: DataTypes.DATE,
      allowNull: false,
      defaultValue: Sequelize.Sequelize.literal('CURRENT_TIMESTAMP')
    }
  }, {
    sequelize,
    tableName: 't_proxy_router',
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
        name: "proxy_router",
        unique: true,
        using: "BTREE",
        fields: [
          { name: "proxy_name" },
          { name: "router_name" },
        ]
      },
    ]
  });
};
