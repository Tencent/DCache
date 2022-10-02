const Sequelize = require('sequelize');
module.exports = function(sequelize, DataTypes) {
  return sequelize.define('t_dbaccess_app', {
    id: {
      autoIncrement: true,
      type: DataTypes.INTEGER,
      allowNull: false,
      primaryKey: true
    },
    dbaccess_name: {
      type: DataTypes.STRING(128),
      allowNull: false,
      defaultValue: ""
    },
    app_name: {
      type: DataTypes.STRING(64),
      allowNull: false,
      defaultValue: ""
    }
  }, {
    sequelize,
    tableName: 't_dbaccess_app',
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
        name: "ro_app",
        unique: true,
        using: "BTREE",
        fields: [
          { name: "dbaccess_name" },
          { name: "app_name" },
        ]
      },
    ]
  });
};
