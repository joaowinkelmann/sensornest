<template>
  <v-app>
    <v-navigation-drawer v-model="drawer" app clipped>
      <v-list density="compact">
        <v-list-item
          prepend-icon="$home"
          title="Home"
          to="/"
        />
      </v-list>
    </v-navigation-drawer>

    <!--
    <v-app-bar app clipped-left color="blue-grey-darken-3">
      <v-app-bar-nav-icon @click="drawer = !drawer" />
      <v-app-bar-title>Dashboard</v-app-bar-title>
    </v-app-bar> -->

    <v-main>
      <v-container fill-height fluid>
        <router-view />
      </v-container>
    </v-main>

  </v-app>
</template>

<script setup>
  import { ref } from 'vue'

  const drawer = ref(null)
</script>
