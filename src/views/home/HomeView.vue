<template>
  <q-page class="q-pa-md">
    <div class="row q-col-gutter-md">
      <div class="col-12 col-md-6 col-lg-4" v-for="item in dashboardItems" :key="item.title">
        <q-card class="cursor-pointer" @click="navigate(item.route)">
          <q-card-section>
            <div class="text-h6">{{ item.title }}</div>
            <div class="text-subtitle2">{{ item.caption }}</div>
          </q-card-section>
          <q-card-actions align="right">
            <q-btn flat color="primary" :label="item.actionLabel" />
          </q-card-actions>
        </q-card>
      </div>
    </div>
  </q-page>
</template>

<script setup>
import { useRouter } from 'vue-router';

const router = useRouter();

const dashboardItems = [
  {
    title: 'Meshes',
    caption: 'Admin for defining and editing the logged users',
    actionLabel: 'Go to Meshes',
    route: '/meshes' // Placeholder
  },
  {
    title: 'My reports',
    caption: 'Personalized reports for device readings in meshes',
    actionLabel: 'Go to Reports',
    route: '/reports' // Placeholder
  },
  {
    title: 'View data',
    caption: 'For future heatmap and interactive reading analysis',
    actionLabel: 'Go to Data View',
    route: '/data' // Placeholder
  },
  {
    title: 'Map editor',
    caption: 'The currently implemented Leaflet vertex editor for Zones',
    actionLabel: 'Go to Map Editor',
    route: '/map-editor'
  }
];

const navigate = (route) => {
  if (route) {
    router.push(route);
  }
};
</script>
