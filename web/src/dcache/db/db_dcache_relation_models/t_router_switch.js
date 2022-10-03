const Sequelize = require('sequelize');
module.exports = function(sequelize, DataTypes) {
  return sequelize.define('t_router_switch', {
    id: {
      autoIncrement: true,
      type: DataTypes.INTEGER,
      allowNull: false,
      primaryKey: true
    },
    app_name: {
      type: DataTypes.STRING(128),
      allowNull: false
    },
    module_name: {
      type: DataTypes.STRING(128),
      allowNull: false
    },
    group_name: {
      type: DataTypes.STRING(128),
      allowNull: true
    },
    master_server: {
      type: DataTypes.STRING(128),
      allowNull: true
    },
    slave_server: {
      type: DataTypes.STRING(128),
      allowNull: true
    },
    mirror_idc: {
      type: DataTypes.STRING(128),
      allowNull: true
    },
    master_mirror: {
      type: DataTypes.STRING(128),
      allowNull: true
    },
    slave_mirror: {
      type: DataTypes.STRING(128),
      allowNull: true
    },
    switch_type: {
      type: DataTypes.INTEGER,
      allowNull: false
    },
    switch_result: {
      type: DataTypes.INTEGER,
      allowNull: false,
      defaultValue: 0
    },
    access_status: {
      type: DataTypes.INTEGER,
      allowNull: false,
      defaultValue: 0
    },
    comment: {
      type: DataTypes.STRING(256),
      allowNull: true
    },
    switch_time: {
      type: DataTypes.DATE,
      allowNull: true
    },
    modify_time: {
      type: DataTypes.DATE,
      allowNull: false,
      defaultValue: Sequelize.Sequelize.literal('CURRENT_TIMESTAMP')
    },
    switch_property: {
      type: DataTypes.ENUM('auto','manual'),
      allowNull: true,
      defaultValue: "manual"
    }
  }, {
    sequelize,
    tableName: 't_router_switch',
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
    ]
  });
};
