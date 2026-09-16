<template>
  <aside class="sidebar">
    <div class="brand">WRS Debugger<span>Wireless Remote Stop</span></div>
    <el-menu :default-active="$route.path" router
      ><el-menu-item index="/connection">连接</el-menu-item
      ><el-menu-item index="/box">无线急停盒</el-menu-item
      ><el-menu-item index="/receiver">机器人接收板</el-menu-item></el-menu
    >
  </aside>
</template>
<style scoped>
.sidebar {
  width: 232px;
  background: #fff;
  border-right: 1px solid var(--app-border);
  flex: none;
}
.brand {
  height: 56px;
  padding: 12px 18px;
  font-size: 16px;
  font-weight: 650;
  color: #1f2d3d;
  border-bottom: 1px solid var(--app-border);
}
.brand span {
  display: block;
  margin-top: 3px;
  font-size: 11px;
  font-weight: 400;
  color: var(--app-muted);
}
.el-menu {
  border-right: 0;
  padding-top: 10px;
}
.el-menu-item {
  margin: 3px 8px;
  border-radius: 5px;
}
</style>
