const Sequelize = require('sequelize');
module.exports = function(sequelize, DataTypes) {
  return sequelize.define('t_config_table', {
    id: {
      autoIncrement: true,
      type: DataTypes.INTEGER,
      allowNull: false,
      primaryKey: true
    },
    config_id: {
      type: DataTypes.INTEGER,
      allowNull: false,
      defaultValue: 0
    },
    server_name: {
      type: DataTypes.STRING(128),
      allowNull: true
    },
    host: {
      type: DataTypes.STRING(20),
      allowNull: true
    },
    item_id: {
      type: DataTypes.INTEGER,
      allowNull: false,
      defaultValue: 0
    },
    config_value: {
      type: DataTypes.TEXT,
      allowNull: true
    },
    level: {
      type: DataTypes.INTEGER,
      allowNull: true
    },
    posttime: {
      type: DataTypes.DATE,
      allowNull: true
    },
    lastuser: {
      type: DataTypes.STRING(50),
      allowNull: true
    },
    config_flag: {
      type: DataTypes.INTEGER,
      allowNull: true
    }
  }, {
    sequelize,
    tableName: 't_config_table',
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
        name: "config_item_new",
        unique: true,
        using: "BTREE",
        fields: [
          { name: "config_id" },
          { name: "item_id" },
          { name: "server_name" },
        ]
      },
      {
        name: "item",
        using: "BTREE",
        fields: [
          { name: "item_id" },
        ]
      },
      {
        name: "server",
        using: "BTREE",
        fields: [
          { name: "server_name" },
          { name: "host" },
        ]
      },
      {
        name: "item_id",
        using: "BTREE",
        fields: [
          { name: "item_id" },
        ]
      },
    ]
  });
};
