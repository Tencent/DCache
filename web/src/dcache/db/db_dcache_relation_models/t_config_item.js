const Sequelize = require('sequelize');
module.exports = function(sequelize, DataTypes) {
  return sequelize.define('t_config_item', {
    id: {
      autoIncrement: true,
      type: DataTypes.INTEGER,
      allowNull: false,
      primaryKey: true
    },
    remark: {
      type: DataTypes.STRING(128),
      allowNull: true
    },
    item: {
      type: DataTypes.STRING(128),
      allowNull: true
    },
    path: {
      type: DataTypes.STRING(128),
      allowNull: true
    },
    reload: {
      type: DataTypes.STRING(4),
      allowNull: true
    },
    period: {
      type: DataTypes.STRING(4),
      allowNull: true,
      defaultValue: "U"
    }
  }, {
    sequelize,
    tableName: 't_config_item',
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
        name: "item_path",
        unique: true,
        using: "BTREE",
        fields: [
          { name: "item" },
          { name: "path" },
        ]
      },
    ]
  });
};
