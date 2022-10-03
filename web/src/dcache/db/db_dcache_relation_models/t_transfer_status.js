const Sequelize = require('sequelize');
module.exports = function(sequelize, DataTypes) {
  return sequelize.define('t_transfer_status', {
    id: {
      autoIncrement: true,
      type: DataTypes.INTEGER,
      allowNull: false,
      primaryKey: true
    },
    app_name: {
      type: DataTypes.STRING(50),
      allowNull: false,
      defaultValue: ""
    },
    module_name: {
      type: DataTypes.STRING(50),
      allowNull: false,
      defaultValue: ""
    },
    src_group: {
      type: DataTypes.STRING(128),
      allowNull: false,
      defaultValue: ""
    },
    dst_group: {
      type: DataTypes.STRING(120),
      allowNull: false,
      defaultValue: ""
    },
    status: {
      type: DataTypes.TINYINT,
      allowNull: false,
      defaultValue: 0
    },
    router_transfer_id: {
      type: DataTypes.TEXT,
      allowNull: true
    },
    auto_updatetime: {
      type: DataTypes.DATE,
      allowNull: false,
      defaultValue: Sequelize.Sequelize.literal('CURRENT_TIMESTAMP')
    },
    transfer_start_time: {
      type: DataTypes.STRING(32),
      allowNull: true
    }
  }, {
    sequelize,
    tableName: 't_transfer_status',
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
        name: "transfer_record",
        unique: true,
        using: "BTREE",
        fields: [
          { name: "app_name" },
          { name: "module_name" },
          { name: "src_group" },
          { name: "dst_group" },
        ]
      },
    ]
  });
};
