angular.module('starter.controllers', [])

.controller('AlarmCtrl', function($scope) {})

.controller('SensorsCtrl', function ($scope) {
    $scope.settings = {
        enableSmoke: true
    };
})

.controller('ChatDetailCtrl', function($scope, $stateParams, Chats) {
  $scope.chat = Chats.get($stateParams.chatId);
})

.controller('SettingsCtrl', function ($scope) {
  $scope.settings = {
    enableFriends: true
  };
});
