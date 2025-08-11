<template>
  <div class="home-view">
    <h1>Welcome to TempMesh Home!</h1>
    <div>
      <router-link to="/login">
        <button>Login</button>
      </router-link>
      <router-link to="/dashboard">
        <button>Go to Dashboard</button>
      </router-link>
    </div>
  </div>
</template>

<script>
export default {
  name: 'HomeView',
};
</script>

<style scoped>
.home-view {
  text-align: center;
  margin-top: 50px;
}
button {
  margin: 10px;
  padding: 10px 20px;
  font-size: 16px;
}
</style>