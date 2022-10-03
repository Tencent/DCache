const Sequelize = require('sequelize');
module.exports = function(sequelize, DataTypes) {
  return sequelize.define('t_idc_map', {
    id: {
      autoIncrement: true,
      type: DataTypes.INTEGER,
      allowNull: false,
      primaryKey: true
    },
    city: {
      type: DataTypes.STRING(255),
      allowNull: false,
      comment: "city name"
    },
    idc: {
      type: DataTypes.STRING(32),
      allowNull: false,
      comment: "idc area"
    }
  }, {
    sequelize,
    tableName: 't_idc_map',
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
