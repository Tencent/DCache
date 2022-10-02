const Sequelize = require('sequelize');
module.exports = function(sequelize, DataTypes) {
  return sequelize.define('t_proxy_app', {
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
    app_name: {
      type: DataTypes.STRING(128),
      allowNull: false,
      defaultValue: ""
    }
  }, {
    sequelize,
    tableName: 't_proxy_app',
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
        name: "proxy_app",
        unique: true,
        using: "BTREE",
        fields: [
          { name: "proxy_name" },
          { name: "app_name" },
        ]
      },
    ]
  });
};
