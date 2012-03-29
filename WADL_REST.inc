<?php

class WADLFunctionNotFound extends Exception {

}

class WADL_REST {
  /**
   * A list of methods present in the WADL, in the format:
   *   array(
   *     'method_name' => array(
   *       'method' => [POST|PUT|GET|DELETE],
   *       'path' => 'full_path',
   *       'params' => array(
   *         'param_name' => array(
   *           'style' => [template/query],
   *           'type' => [string/whatever],
   *         ),
   *       )
   *     ),
   *   )
   */
   
  protected $wadl_url = NULL;
  protected $username = NULL;
  protected $password = NULL;
  
  protected $simple_xml_wadl = NULL;
  
  protected $methods = array();
  
  public function __construct($params) {
    $this->wadl_url = $params['wadl_url'];
    $this->username = $params['username'];
    $this->password = $params['password'];
    
    $this->_parse_wadl();
  }
  
  private function _get_wadl() {
    if (!$this->simple_xml_wadl) {
      //Get the WADL (use credentials, if provided), and load into simple_xml_wadl.
      $wadl = $this->_curl_call($this->wadl_url);
      $this->simple_xml_wadl = simplexml_load_string($wadl);
    }
    
    return $this->simple_xml_wadl;
  }
  
  /**
   *  Kicks off the (recursive) parsing of the WADL file
   */
  private function _parse_wadl() {
    $wadl = $this->_get_wadl();
    
    $resources = $wadl->resources;
    $base = (string)$resources->attributes()->base;
    
    //Remove a final slash(es)...
    while (substr($base, -1) === '/') {
      $base = substr($base, 0, -1);
    }
    
    $this->_parse_child_resource($base, $this->_parse_params($resources), $resources);
  }
  
  /**
   *  Parse the WADL resources recursively, building an array of methods.
   */
  private function _parse_child_resource($current_resource_path, array $current_params, SimpleXMLElement $current_xml_node) {
    //dsm(func_get_args(), 'parse child resource args');
    //Recursive part; get all the child resources
    foreach ($current_xml_node->resource as $res) {
      $sub_path = (string)$res->attributes()->path;
      $new_path = $current_resource_path .
        ((strpos($sub_path, '/') === 0)?
          '':
          '/') .
        $sub_path;
      $new_params = array_merge($current_params, $this->_parse_params($res));
      
      $this->_parse_child_resource($new_path, $new_params, $res);
    }
    
    //Build the methods under each resource.
    foreach ($current_xml_node->method as $meth) {
      $this->methods[(string)$meth->attributes()->id] = array(
        'path' => $current_resource_path,
        'method' => (string)$meth->attributes()->name,
        'params' => array_merge($current_params, $this->_parse_params($meth->request)),
      );
    }
  }
  
  private function _parse_params($xml_element) {
    $params = array();

    if ($xml_element->param) {
      foreach ($xml_element->param as $par) {
        $par = $par->attributes();
        
        $params[(string)$par->name] = array(
          'style' => (string)$par->style,
          'type' => (string)$par->type,
        );      
      }
    }
    
    return $params;
  }
  
  /**
   *  Implementation of PHP's magic __call function to make calls to the REST API
   */
  public function __call($function, array $args) {
    if (array_key_exists($function, $this->methods)) {
      return $this->_call_helper($function, (array)$args[0]);
    }
    else {
      throw new WADLFunctionNotFound('Method not defined in WADL!');
    }
  }
  
  /**
   *  Allow additional curlopts to be added by subclasses...  I'm thinking of using the COOKIEJAR stuff to maintain session info somewhere?
   */
  protected function _get_curl_options() {
    $toReturn = array(
      CURLOPT_RETURNTRANSFER => TRUE,
    );
    
    if (!empty($this->username) && !empty($this->password)) {
      $toReturn[CURLOPT_USERPWD] = "{$this->username}:{$this->password}";
    }
    
    return $toReturn;
  }
  
  private function _curl_call($url, array $options = array()) {
    $ch = curl_init($url);
    
    $options += $this->_get_curl_options();
    
    curl_setopt_array($ch, $options);
    
    $toReturn = curl_exec($ch);
    
    //TODO:  Probably want to check the return code or the like, and return exceptions or sommat.
    //dsm(curl_getinfo($ch), 'curl info');
    //dsm($options, 'options');
    //dsm($toReturn, 'output');
    
    curl_close($ch);
    
    return $toReturn;
  }
  
  private function _call_helper($function, array $params) {
    //assert(array_key_exists($function, $this->methods));
    
    $method = $this->methods[$function];
    $spec_params = array_intersect_key($method['params'], $params);
    
    $query_params = array();
    foreach ($spec_params as $name => $info) {
      switch ($info['style']) {
        case 'template':
          //Replace the given section in the method's URL
          $method['path'] = preg_replace("/\{$name\}/", $params[$name], $method['path']);
        break;
        case 'query':
          //Throw in the query_params array for later use
          $query_params[$name] = $params[$name];
        break;
        default:
          throw new Exception('Unknown parameter style!');
      }
    }
    
    $curl_map = array(
      'POST' => CURLOPT_POST,
      'GET' => CURLOPT_HTTPGET,
      'PUT' => CURLOPT_PUT,
      'DELETE' => CURLOPT_DELETE,
    );
    
    $curl_options = array(
      $curl_map[$method['method']] => TRUE,
    );
    
    return $this->_curl_call($method['path'] . '?' . http_build_query($query_params), $curl_options);
  }
}
