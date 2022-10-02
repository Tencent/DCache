const Sequelize = require('sequelize');
module.exports = function(sequelize, DataTypes) {
  return sequelize.define('t_proxy_cache', {
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
    cache_name: {
      type: DataTypes.STRING(128),
      allowNull: false,
      defaultValue: ""
    },
    modify_time: {
      type: DataTypes.DATE,
      allowNull: false,
      defaultValue: Sequelize.Sequelize.literal('CURRENT_TIMESTAMP')
    }
  }, {
    sequelize,
    tableName: 't_proxy_cache',
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
        name: "proxy_cache",
        unique: true,
        using: "BTREE",
        fields: [
          { name: "proxy_name" },
          { name: "cache_name" },
        ]
      },
    ]
  });
};
