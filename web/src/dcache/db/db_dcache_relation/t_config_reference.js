const Sequelize = require('sequelize');
module.exports = function(sequelize, DataTypes) {
  return sequelize.define('t_config_reference', {
    id: {
      autoIncrement: true,
      type: DataTypes.INTEGER,
      allowNull: false,
      primaryKey: true
    },
    reference_id: {
      type: DataTypes.INTEGER,
      allowNull: true
    },
    server_name: {
      type: DataTypes.STRING(128),
      allowNull: true
    },
    host: {
      type: DataTypes.STRING(20),
      allowNull: true
    }
  }, {
    sequelize,
    tableName: 't_config_reference',
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
        name: "reference_id",
        unique: true,
        using: "BTREE",
        fields: [
          { name: "server_name" },
          { name: "host" },
        ]
      },
    ]
  });
};
