%include('header.tpl', title="", refresh=10)

%include('menu.tpl', current='stop')

%if len(active) == 0:

<p>No active Songcast Receivers found</p>

%else:

<form action="/stop" method="POST">
<table>
  <tr>
    <th>Select</th>
    <th>Name</th>
    <th>State</th>
    <th>UUID</th>
    <th>Sender URI</th>
  </tr>
%for receiver in active:
  <tr>
    <td><input type="checkbox" name="Stop" value="{{receiver[2]}}"/></td>
    <td>{{receiver[0]}}</td>
    <td>{{receiver[1]}}</td>
    <td>{{receiver[2]}}</td>
    <td>{{receiver[3]}}</td>
  </tr>
%end
</table>
<input type="submit">
</form>

%end

%include('footer.tpl')
