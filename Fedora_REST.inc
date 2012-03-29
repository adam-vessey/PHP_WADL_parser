<?php
require_once(dirname(__FILE__) . '/WADL_REST.inc');

class Fedora_REST extends WADL_REST {
//Yep...  Not really doing much different at the moment...
  
  public function get_methods() {
    return $this->methods;
  }
}
