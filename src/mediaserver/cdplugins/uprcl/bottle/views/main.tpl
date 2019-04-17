%include('header', title=title, reloadsecs=reloadsecs)

<div id="fade"></div>
<div id="results">

<h3>{{friendlyname}} is {{status}}</h3>
<form action="" method="post">
<input type="submit" name="what" value="Refresh Status"><br/>
<input type="submit" name="what" value="Update Index"><br/>
<input type="submit" name="what" value="Reset Index"
       onclick="return confirm('Rebuilding the index may take a long time. Confirm ?');"><br/>

</div>
</div>

%include('footer')
