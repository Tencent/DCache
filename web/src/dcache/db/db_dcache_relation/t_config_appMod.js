const Sequelize = require('sequelize');
module.exports = function(sequelize, DataTypes) {
  return sequelize.define('t_config_appMod', {
    id: {
      autoIncrement: true,
      type: DataTypes.INTEGER,
      allowNull: false,
      primaryKey: true
    },
    appName: {
      type: DataTypes.STRING(128),
      allowNull: true
    },
    moduleName: {
      type: DataTypes.STRING(128),
      allowNull: true
    },
    cacheType: {
      type: DataTypes.STRING(16),
      allowNull: true
    }
  }, {
    sequelize,
    tableName: 't_config_appMod',
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
        name: "app_module",
        unique: true,
        using: "BTREE",
        fields: [
          { name: "appName" },
          { name: "moduleName" },
        ]
      },
    ]
  });
};
