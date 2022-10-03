const Sequelize = require('sequelize');
module.exports = function(sequelize, DataTypes) {
  return sequelize.define('t_router_app', {
    id: {
      autoIncrement: true,
      type: DataTypes.INTEGER,
      allowNull: false,
      primaryKey: true
    },
    router_name: {
      type: DataTypes.STRING(128),
      allowNull: false,
      defaultValue: ""
    },
    app_name: {
      type: DataTypes.STRING(128),
      allowNull: false,
      defaultValue: ""
    },
    host: {
      type: DataTypes.CHAR(50),
      allowNull: true
    },
    port: {
      type: DataTypes.CHAR(10),
      allowNull: true
    },
    user: {
      type: DataTypes.CHAR(50),
      allowNull: true
    },
    password: {
      type: DataTypes.CHAR(50),
      allowNull: true
    },
    charset: {
      type: DataTypes.CHAR(10),
      allowNull: true,
      defaultValue: "utf8mb4"
    },
    db_name: {
      type: DataTypes.CHAR(50),
      allowNull: true
    }
  }, {
    sequelize,
    tableName: 't_router_app',
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
        name: "router_app",
        unique: true,
        using: "BTREE",
        fields: [
          { name: "router_name" },
          { name: "app_name" },
        ]
      },
    ]
  });
};
